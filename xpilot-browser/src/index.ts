// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import {
  createBrowserTextProvider,
  type BrowserTextProvider,
} from './browser-text';
import { createBrowserSocket, sameOriginEndpoint } from './browser-socket';
import {
  loadGameRuntime,
  withGameString,
  type GameRuntime,
} from './game-runtime';
import './style.css';
import { installMapAssets } from './map-assets';

const canvas = document.querySelector<HTMLCanvasElement>('#game')!;
const viewport = document.querySelector<HTMLElement>('#viewport')!;
const status = document.querySelector<HTMLOutputElement>('#status')!;
const connection = document.querySelector<HTMLOutputElement>('#connection')!;
const connect = document.querySelector<HTMLButtonElement>('#connect')!;
const disconnect = document.querySelector<HTMLButtonElement>('#disconnect')!;
const nickname = document.querySelector<HTMLInputElement>('#nickname')!;
let runtime: GameRuntime | undefined;
let fonts: BrowserTextProvider | undefined;
let phase: 'idle' | 'admission' | 'assets' | 'game' | 'resume' | 'quit' =
  'idle';
let assetRequest = new AbortController();
let resumeUntil = 0;
let attemptUntil = 0;
let quitUntil = 0;
let retryTimer = 0;
let retryPending = false;
let animation = 0;
let generation = 0;

const resize = () =>
  runtime?._xp_game_resize(
    Math.max(128, viewport.clientWidth),
    Math.max(128, viewport.clientHeight)
  );
const observer = new ResizeObserver(resize);
observer.observe(viewport);
const stop = (message: string, state: string) => {
  generation++;
  assetRequest.abort();
  clearTimeout(retryTimer);
  retryPending = false;
  cancelAnimationFrame(animation);
  phase = 'idle';
  runtime?._xp_browser_session_close();
  runtime?._xp_game_stop();
  runtime = undefined;
  connection.textContent = message;
  connection.dataset.state = state;
  connect.disabled = false;
  disconnect.disabled = true;
};
const openSocket = () =>
  createBrowserSocket(
    new WebSocket(sameOriginEndpoint(location.href), 'xpilot-infinity-v1'),
    () => queueMicrotask(advance)
  );
const resumeAttempt = () => {
  if (!runtime || phase !== 'resume') return;
  retryPending = false;
  runtime.xpilotSocket = openSocket();
  attemptUntil = performance.now() + 5000;
  if (runtime._xp_game_resume_begin() < 0)
    stop('Cannot resume this session', 'error');
};
const loadMap = async (game: GameRuntime) => {
  const current = generation;
  assetRequest = new AbortController();
  const request = assetRequest;
  const timeout = window.setTimeout(() => request.abort(), 15000);
  try {
    await installMapAssets(
      game.FS,
      game.UTF8ToString(game._xp_game_map_url()),
      location.href,
      assetRequest.signal
    );
    if (current !== generation) return;
    if (game._xp_game_assets_ready() !== 0)
      throw new Error('Unable to initialize map');
    phase = 'game';
  } catch (error) {
    if (current === generation) stop(String(error), 'error');
  } finally { clearTimeout(timeout); }
};
const advance = () => {
  if (!runtime) return;
  try {
    if (phase === 'quit') {
      if (runtime._xp_game_quit_step() > 0 || performance.now() >= quitUntil)
        stop('Disconnected', 'disconnected');
      return;
    }
    if (phase === 'resume') {
      if (performance.now() >= resumeUntil) {
        stop('Reconnection timed out', 'error');
        return;
      }
      if (retryPending) return;
      const result = runtime._xp_game_resume_step();
      if (result === 1) {
        phase = 'game';
        connection.dataset.state = 'playing';
        connection.textContent = 'Playing';
      } else if (result < 0 || performance.now() >= attemptUntil) {
        runtime.xpilotSocket?.dispose();
        retryPending = true;
        retryTimer = window.setTimeout(resumeAttempt, 500);
        return;
      } else return;
    }
    if (phase === 'admission') {
      const result = runtime._xp_browser_session_step();
      if (result < 0) {
        stop(
          runtime.UTF8ToString(runtime._xp_browser_session_error()),
          'error'
        );
        return;
      }
      if (result > 0) {
        if (
          withGameString(runtime, nickname.value, runtime._xp_game_begin) !== 0
        )
          throw new Error('Unable to initialize game session');
        phase = 'game';
        connection.textContent = 'Loading map…';
        connection.dataset.state = 'setup';
      }
    }
    if (phase === 'game') {
      const result = runtime._xp_game_step();
      if (result === 2) {
        phase = 'assets';
        queueMicrotask(async () => {
          if (runtime) await loadMap(runtime);
        });
        return;
      }
      if (result < 0) {
        if (
          connection.dataset.state === 'playing' &&
          runtime.xpilotSocket?.state() === 'closed'
        ) {
          runtime._xp_game_clear_input();
          phase = 'resume';
          resumeUntil = performance.now() + 30000;
          connection.dataset.state = 'reconnecting';
          connection.textContent = 'Reconnecting…';
          resumeAttempt();
        } else stop('Game ended. Join to play again.', 'disconnected');
        return;
      }
      if (result > 0) {
        connection.textContent = 'Playing';
        connection.dataset.state = 'playing';
      }
      canvas.dataset.frame = String(runtime._xp_game_frame());
      canvas.dataset.shipX = String(runtime._xp_game_position(0));
      canvas.dataset.shipY = String(runtime._xp_game_position(1));
      canvas.dataset.heading = String(runtime._xp_game_heading());
      canvas.dataset.shots = String(runtime._xp_game_shots());
      document.querySelector('#chat-message')!.textContent =
        runtime.UTF8ToString(runtime._xp_game_message());
    }
  } catch (error) {
    stop(String(error), 'error');
  }
};
const tick = () => {
  advance();
  if (runtime && phase !== 'idle') animation = requestAnimationFrame(tick);
};
const prepare = async () => {
  const current = ++generation;
  if (!fonts) throw new Error('Fonts are unavailable');
  const created = await loadGameRuntime(canvas, fonts);
  if (current !== generation) {
    created._xp_game_stop();
    return false;
  }
  runtime = created;
  if (runtime._xp_game_init() !== 0)
    throw new Error('SDL / WebGL 2 initialization failed');
  resize();
  return true;
};
const join = async () => {
  connect.disabled = true;
  connection.textContent = 'Connecting…';
  connection.dataset.state = 'connecting';
  try {
    if (!runtime && !(await prepare())) return;
    if (!runtime) return;
    runtime.xpilotSocket = openSocket();
    if (
      withGameString(runtime, nickname.value, (pointer) =>
        runtime!._xp_browser_session_begin(pointer)
      ) < 0
    )
      throw new Error('Cannot start admission');
    phase = 'admission';
    connection.textContent = 'Connecting…';
    connection.dataset.state = 'connecting';
    disconnect.disabled = false;
    canvas.focus();
    animation = requestAnimationFrame(tick);
  } catch (error) {
    stop(String(error), 'error');
  }
};
document.querySelector('#join')!.addEventListener('submit', async (event) => {
  event.preventDefault();
  await join();
});
disconnect.addEventListener('click', () => {
  if (phase !== 'game') {
    stop('Disconnected', 'disconnected');
    return;
  }
  phase = 'quit';
  quitUntil = performance.now() + 5000;
  disconnect.disabled = true;
  connection.textContent = 'Leaving game…';
  connection.dataset.state = 'leaving';
  advance();
});
document.querySelector('#chat-form')!.addEventListener('submit', (event) => {
  event.preventDefault();
  const input = document.querySelector<HTMLInputElement>('#chat')!;
  if (runtime && connection.dataset.state === 'playing' && input.value) {
    withGameString(runtime, input.value, (pointer) =>
      runtime!._xp_game_chat(pointer)
    );
    input.value = '';
    canvas.focus();
  }
});
window.addEventListener('pagehide', () => {
  stop('Disconnected', 'disconnected');
});
try {
  fonts = await createBrowserTextProvider(
    new URL('fonts/VeraMoBd.ttf', document.baseURI).href,
    new URL('fonts/FreeSansBoldOblique.ttf', document.baseURI).href
  );
  if (await prepare()) {
    status.textContent = 'Ready to join';
    status.dataset.state = 'ready';
    connect.disabled = false;
  }
} catch (error) {
  status.textContent = String(error);
  status.dataset.state = 'error';
}
