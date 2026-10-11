import { assertNodeRuntime } from './runtime';
import { isAbsolute, parse, relative, resolve, sep } from 'node:path';
import { lookup } from 'node:dns/promises';
import { isIPv4 } from 'node:net';
import { hostname, userInfo } from 'node:os';

interface WasmModule {
  _malloc: (size: number) => number;
  _free: (pointer: number) => void;
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  _xp_server_start: (argc: number, argv: number) => number;
  _xp_server_activate: () => number;
  _xp_server_resolve_name: (index: number) => number;
  _xp_server_pump: (frame: number) => number;
  _xp_server_stop: () => void;
  _xp_server_close_socket: (pointer: number) => void;
  _xp_server_frames: () => number;
  _xp_server_robots: () => number;
  _xp_server_fps: () => number;
  _xp_server_status: () => number;
  NODEFS: unknown;
  UTF8ToString: (pointer: number) => string;
  FS: {
    mkdir: (path: string) => void;
    mount: (type: unknown, options: { root: string }, path: string) => void;
    chdir: (path: string) => void;
  };
}

/** Paths and host callbacks for one isolated WASM game. */
export interface CoreOptions {
  /** Built Emscripten ES module. */
  runtime: URL;
  /** Absolute directory containing packaged game data. */
  assets: string;
  /** Absolute writable directory owned by the server operator. */
  data: string;
  /** Optional directory containing the built browser application. */
  web: string | undefined;
  /** Schedule a host turn when a socket becomes ready. */
  ready: () => void;
  /** Receive a stdout or stderr line from the game. */
  log: (line: string) => void;
}

/** One game instance, owned and advanced by its host. */
export interface ServerCore {
  /** Initialize once from native server arguments, returning an exit code. */
  start: (args: readonly string[]) => Promise<number>;
  /** Dispatch ready input; advance one frame when requested. False means stopped. */
  pump: (frame: boolean) => boolean;
  /** Whether the last pump dispatched descriptors and more input may be ready. */
  hasInput: () => boolean;
  /** Stop idempotently, saving state and closing listeners. */
  stop: () => Promise<void>;
  /** Completed frames. */
  frames: () => number;
  /** Current robot count. */
  robots: () => number;
  /** Configured game frames per second. */
  fps: () => number;
  /** Last startup or exit status. */
  exitCode: () => number;
  /** Map an operator-supplied host path into the virtual filesystem. */
  hostPath: (path: string) => string;
}

/**
 * Load an independent game module and mount its assets and writable data.
 * @param options Host paths and callbacks; no game is started during loading.
 * @returns Core whose resources must be released with stop after startup.
 */
export const createCore = async (options: CoreOptions): Promise<ServerCore> => {
  assertNodeRuntime();
  const { default: create } = await import(
    /* @vite-ignore */ options.runtime.href
  );
  const addresses = new Map<string, string>();
  const closing = new Set<Promise<void>>();
  const wasm: WasmModule = await create({
    noInitialRun: true,
    onSocketReady: options.ready,
    print: options.log,
    printErr: options.log,
    userName: userInfo().username,
    hostName: hostname(),
    browserRoot: options.web === undefined ? undefined : '/.xpilot-wasm/web',
    lookupAddress: (name: string) => addresses.get(name),
    deferSocketClose: (pointer: number) => {
      const closed = new Promise<void>((done) => {
        setImmediate(() => {
          wasm._xp_server_close_socket(pointer);
          closing.delete(closed);
          done();
        });
      });
      closing.add(closed);
    },
    mapPath: (path: string) => {
      if (
        !isAbsolute(path) ||
        ['/.xpilot-wasm', '/dev', '/proc'].some(
          (root) => path === root || path.startsWith(`${root}/`)
        )
      )
        return path.split(sep).join('/');
      return hostPath(path);
    },
  });
  wasm.FS.mkdir('/.xpilot-wasm');
  wasm.FS.mkdir('/.xpilot-wasm/assets');
  wasm.FS.mount(wasm.NODEFS, { root: options.assets }, '/.xpilot-wasm/assets');
  wasm.FS.mkdir('/.xpilot-wasm/data');
  wasm.FS.mount(wasm.NODEFS, { root: options.data }, '/.xpilot-wasm/data');
  wasm.FS.chdir('/.xpilot-wasm/data');
  if (options.web !== undefined) {
    wasm.FS.mkdir('/.xpilot-wasm/web');
    wasm.FS.mount(wasm.NODEFS, { root: options.web }, '/.xpilot-wasm/web');
  }
  wasm.FS.mkdir('/.xpilot-wasm/host');
  const roots = new Map<string, string>();
  const hostPath = (path: string): string => {
    const absolute = resolve(path);
    const root = parse(absolute).root;
    let mount = roots.get(root);
    if (mount === undefined) {
      mount = `/.xpilot-wasm/host/${roots.size}`;
      roots.set(root, mount);
      wasm.FS.mkdir(mount);
      wasm.FS.mount(wasm.NODEFS, { root }, mount);
    }
    return `${mount}/${relative(root, absolute).split(sep).join('/')}`;
  };
  let ready = 0;
  return {
    start: async (args) => {
      const allocated: number[] = [];
      try {
        const strings = ['xpilot-infinity', ...args].map((arg) => {
          const bytes = new TextEncoder().encode(arg + '\0');
          const pointer = wasm._malloc(bytes.length);
          if (!pointer) throw new Error('Out of WASM memory');
          allocated.push(pointer);
          wasm.HEAPU8.set(bytes, pointer);
          return pointer;
        });
        const argv = wasm._malloc((strings.length + 1) * 4);
        if (!argv) throw new Error('Out of WASM memory');
        allocated.push(argv);
        wasm.HEAPU32.set([...strings, 0], argv / 4);
        const result = wasm._xp_server_start(strings.length, argv);
        if (result !== 0) return result;
        const names = new Set<string>();
        for (let index = 0; ; index++) {
          const pointer = wasm._xp_server_resolve_name(index);
          if (pointer !== 0) names.add(wasm.UTF8ToString(pointer));
          else if (index > 0) break;
        }
        await Promise.all(
          [...names].map(async (name) => {
            if (isIPv4(name)) {
              addresses.set(name, name);
              return;
            }
            try {
              const result = await lookup(name, { family: 4 });
              addresses.set(name, result.address);
            } catch (error) {
              options.log(`Could not resolve ${name}: ${String(error)}`);
            }
          })
        );
        return wasm._xp_server_activate();
      } finally {
        for (const pointer of allocated) wasm._free(pointer);
      }
    },
    pump: (frame) => {
      ready = wasm._xp_server_pump(Number(frame));
      return ready >= 0;
    },
    hasInput: () => ready > 0,
    stop: async () => {
      wasm._xp_server_stop();
      await Promise.all([...closing]);
    },
    frames: () => wasm._xp_server_frames(),
    robots: () => wasm._xp_server_robots(),
    fps: () => wasm._xp_server_fps(),
    exitCode: () => wasm._xp_server_status(),
    hostPath,
  };
};
