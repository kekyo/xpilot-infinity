import { assertNodeRuntime } from './runtime';

interface WasmModule {
  _malloc: (size: number) => number;
  _free: (pointer: number) => void;
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  _xp_server_start: (argc: number, argv: number) => number;
  _xp_server_pump: (frame: number) => number;
  _xp_server_stop: () => void;
  _xp_server_frames: () => number;
  _xp_server_robots: () => number;
  _xp_server_fps: () => number;
  _xp_server_status: () => number;
  NODEFS: unknown;
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
  /** Schedule a host turn when a socket becomes ready. */
  ready: () => void;
  /** Receive a stdout or stderr line from the game. */
  log: (line: string) => void;
}

/** One game instance, owned and advanced by its host. */
export interface ServerCore {
  /** Initialize once from native server arguments, returning an exit code. */
  start: (args: readonly string[]) => number;
  /** Dispatch ready input; advance one frame when requested. False means stopped. */
  pump: (frame: boolean) => boolean;
  /** Whether the last pump dispatched descriptors and more input may be ready. */
  hasInput: () => boolean;
  /** Stop idempotently, saving state and closing listeners. */
  stop: () => void;
  /** Completed frames. */
  frames: () => number;
  /** Current robot count. */
  robots: () => number;
  /** Configured game frames per second. */
  fps: () => number;
  /** Last startup or exit status. */
  exitCode: () => number;
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
  const wasm: WasmModule = await create({
    noInitialRun: true,
    onSocketReady: options.ready,
    print: options.log,
    printErr: options.log,
  });
  wasm.FS.mkdir('/assets');
  wasm.FS.mount(wasm.NODEFS, { root: options.assets }, '/assets');
  wasm.FS.mkdir('/data');
  wasm.FS.mount(wasm.NODEFS, { root: options.data }, '/data');
  wasm.FS.chdir('/data');
  let ready = 0;
  return {
    start: (args) => {
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
        return wasm._xp_server_start(strings.length, argv);
      } finally {
        for (const pointer of allocated) wasm._free(pointer);
      }
    },
    pump: (frame) => {
      ready = wasm._xp_server_pump(Number(frame));
      return ready >= 0;
    },
    hasInput: () => ready > 0,
    stop: () => wasm._xp_server_stop(),
    frames: () => wasm._xp_server_frames(),
    robots: () => wasm._xp_server_robots(),
    fps: () => wasm._xp_server_fps(),
    exitCode: () => wasm._xp_server_status(),
  };
};
