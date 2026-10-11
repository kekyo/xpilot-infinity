import { createCore, type CoreOptions, type ServerCore } from './core';

/** Running game whose resources belong to the caller. */
export interface RunningServer {
  /** Request an orderly stop. Repeated calls have no effect. */
  stop: () => void;
  /** Exit code, resolved once the game has stopped and timers are removed. */
  closed: Promise<number>;
}

/**
 * Start a game on the Node.js event loop with independent input and frame work.
 * @param options Runtime paths and log receiver; socket readiness is managed here.
 * @param args Native server arguments, excluding the program name.
 * @returns Running server; its owner must await closed and arrange shutdown.
 */
export const startServer = async (
  options: Omit<CoreOptions, 'ready'>,
  args: readonly string[]
): Promise<RunningServer> => {
  let core: ServerCore | undefined;
  let running = false;
  let failed = false;
  let timer: NodeJS.Timeout | undefined;
  let input: NodeJS.Immediate | undefined;
  let finish: (code: number) => void = () => {};
  const closed = new Promise<number>((resolve) => {
    finish = resolve;
  });
  const stop = () => {
    if (!running) return;
    running = false;
    if (timer !== undefined) clearTimeout(timer);
    if (input !== undefined) clearImmediate(input);
    try {
      core!.stop();
      finish(failed ? 1 : core!.exitCode());
    } catch (error) {
      options.log(String(error));
      finish(1);
    }
  };
  const pump = (frame: boolean): boolean => {
    try {
      if (!core!.pump(frame)) {
        stop();
        return false;
      }
      return true;
    } catch (error) {
      options.log(String(error));
      failed = true;
      stop();
      return false;
    }
  };
  const ready = () => {
    if (!running || input !== undefined) return;
    input = setImmediate(() => {
      input = undefined;
      if (running && pump(false) && core!.hasInput()) ready();
    });
  };
  core = await createCore({ ...options, ready });
  const result = core.start(args);
  if (result !== 0) {
    core.stop();
    throw new Error(`Server initialization failed (${result}).`);
  }
  running = true;
  let nextFrame = performance.now() + 1000 / core.fps();
  const schedule = () => {
    if (!running) return;
    timer = setTimeout(
      () => {
        const now = performance.now();
        if (now >= nextFrame) {
          if (!pump(true)) return;
          const interval = 1000 / core!.fps();
          nextFrame += interval;
          if (now - nextFrame > interval * 2) nextFrame = now + interval;
        }
        schedule();
      },
      Math.max(0, Math.ceil(nextFrame - performance.now()))
    );
  };
  ready();
  schedule();
  return { stop, closed };
};
