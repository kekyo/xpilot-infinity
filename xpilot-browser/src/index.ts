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
import { createBrowserAudio, type BrowserAudio } from './browser-audio';
import { loadSettings, saveSettings, type BrowserSettings } from './settings';
import { initializeWithListeners } from './runtime-listeners';

const canvas = document.querySelector<HTMLCanvasElement>('#game')!;
const viewport = document.querySelector<HTMLElement>('#viewport')!;
const status = document.querySelector<HTMLOutputElement>('#status')!;
const connection = document.querySelector<HTMLOutputElement>('#connection')!;
const connect = document.querySelector<HTMLButtonElement>('#connect')!;
const disconnect = document.querySelector<HTMLButtonElement>('#disconnect')!;
const nickname = document.querySelector<HTMLInputElement>('#nickname')!;
const mute = document.querySelector<HTMLInputElement>('#mute')!;
const chat = document.querySelector<HTMLInputElement>('#chat')!;
const settingsStatus = document.querySelector<HTMLOutputElement>('#settings')!;
const audioStatus = document.querySelector<HTMLOutputElement>('#audio')!;
let preferences: BrowserSettings = {
  nickname: 'BrowserPilot',
  muted: false,
  config: '',
};
let audio: BrowserAudio | undefined;
let contextLost = false;
let windowFocused = true;
let runtime: GameRuntime | undefined;
let releaseRuntimeListeners: (() => void) | undefined;
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

const focusGame = () => {
  // Admission and settings can finish while the player is already typing.
  if (
    connection.dataset.state === 'playing' &&
    document.hasFocus() &&
    document.activeElement !== chat
  )
    canvas.focus({ preventScroll: true });
};

const resize = () => {
  if (!runtime || contextLost) return;
  const scale = Math.max(1, Math.min(4, devicePixelRatio));
  if (runtime.xpilotTextScale !== scale) {
    runtime.xpilotTextScale = scale;
    runtime._xp_game_text_scale_changed();
  }
  runtime._xp_game_resize(
    Math.max(
      128,
      document.fullscreenElement ? innerWidth : viewport.clientWidth
    ),
    Math.max(
      128,
      document.fullscreenElement ? innerHeight : viewport.clientHeight
    )
  );
};
const updateAudio = () => {
  if (!audio) return;
  const info = audio.stats();
  audioStatus.dataset.state = info.state;
  audioStatus.dataset.active = String(info.active);
  audioStatus.dataset.peak = String(info.peak);
  audioStatus.textContent = mute.checked
    ? 'Audio muted'
    : info.state === 'running'
      ? 'Audio enabled'
      : 'Audio ready';
};
const persist = async () => {
  try {
    preferences = {
      nickname: nickname.value,
      muted: mute.checked,
      config: runtime
        ? new TextDecoder().decode(
            runtime.FS.readFile('/home/web_user/.xpilotrc')
          )
        : preferences.config,
    };
    await saveSettings(preferences);
    settingsStatus.textContent = 'Settings saved';
    settingsStatus.dataset.state = 'saved';
  } catch (error) {
    settingsStatus.textContent = String(error);
    settingsStatus.dataset.state = 'error';
  }
};
const observer = new ResizeObserver(resize);
observer.observe(viewport);
const stop = (message: string, state: string) => {
  generation++;
  assetRequest.abort();
  audio?.stop();
  updateAudio();
  contextLost = false;
  clearTimeout(retryTimer);
  retryPending = false;
  cancelAnimationFrame(animation);
  phase = 'idle';
  runtime?._xp_browser_session_close();
  runtime?._xp_game_stop();
  releaseRuntimeListeners?.();
  releaseRuntimeListeners = undefined;
  runtime = undefined;
  connection.textContent = message;
  connection.dataset.state = state;
  connect.disabled = false;
  nickname.disabled = false;
  disconnect.disabled = true;
};
const openSocket = () =>
  createBrowserSocket(
    new WebSocket(sameOriginEndpoint(location.href), 'xpilot-infinity-v1'),
    () => {
      // Gameplay renders from tick, once per display frame. Rendering on each
      // message can starve input when a large canvas cannot keep up with the
      // server. Native Net_input already discards superseded game frames.
      // Hidden pages still service the session while animation frames pause.
      if (phase !== 'game' || document.hidden) queueMicrotask(advance);
    }
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
  } finally {
    clearTimeout(timeout);
  }
};
const advance = () => {
  if (!runtime || contextLost) return;
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
        focusGame();
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
      if (result > 0 && connection.dataset.state !== 'playing') {
        connection.textContent = 'Playing';
        connection.dataset.state = 'playing';
        focusGame();
      }
      canvas.dataset.frame = String(runtime._xp_game_frame());
      canvas.dataset.shipX = String(runtime._xp_game_position(0));
      canvas.dataset.shipY = String(runtime._xp_game_position(1));
      canvas.dataset.heading = String(runtime._xp_game_heading());
      canvas.dataset.shots = String(runtime._xp_game_shots());
      canvas.dataset.logicalWidth = String(runtime._xp_game_width());
      canvas.dataset.held = String(runtime._xp_game_held());
      document.querySelector('#chat-message')!.textContent =
        runtime.UTF8ToString(runtime._xp_game_message());
    }
  } catch (error) {
    stop(String(error), 'error');
  }
};
const tick = () => {
  advance();
  updateAudio();
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
  if (!audio) {
    try {
      audio = await createBrowserAudio(created.FS);
    } catch (error) {
      audioStatus.textContent = 'Audio unavailable: ' + String(error);
    }
  }
  if (current !== generation) return false;
  created.xpilotAudio = audio;
  audio?.mute(mute.checked || !windowFocused || document.hidden);
  created.xpilotQuit = leave;
  created.xpilotPointerRequest = (enabled) =>
    queueMicrotask(async () => {
      try {
        if (enabled && document.pointerLockElement !== canvas)
          await canvas.requestPointerLock();
        else if (!enabled && document.pointerLockElement)
          document.exitPointerLock();
      } catch (error) {
        if (runtime === created) created._xp_game_pointer(0);
        connection.textContent =
          'Mouse steering request was declined: ' + String(error);
      }
    });
  created.xpilotSettingsChanged = () =>
    queueMicrotask(async () => {
      if (runtime === created) await persist();
    });
  created.FS.writeFile(
    '/home/web_user/.xpilotrc',
    new TextEncoder().encode(preferences.config)
  );
  const initialized = initializeWithListeners([window, document, canvas], () =>
    created._xp_game_init()
  );
  releaseRuntimeListeners = initialized.dispose;
  if (initialized.value !== 0) {
    releaseRuntimeListeners();
    throw new Error('SDL / WebGL 2 initialization failed');
  }
  resize();
  return true;
};
const join = async () => {
  connect.disabled = true;
  nickname.disabled = true;
  connection.textContent = 'Connecting…';
  connection.dataset.state = 'connecting';
  try {
    await audio?.unlock();
    await persist();
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
    animation = requestAnimationFrame(tick);
  } catch (error) {
    stop(String(error), 'error');
  }
};
document.querySelector('#join')!.addEventListener('submit', async (event) => {
  event.preventDefault();
  await join();
});
const leave = () => {
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
};
disconnect.addEventListener('click', leave);
document.querySelector('#chat-form')!.addEventListener('submit', (event) => {
  event.preventDefault();
  if (runtime && connection.dataset.state === 'playing' && chat.value) {
    withGameString(runtime, chat.value, (pointer) =>
      runtime!._xp_game_chat(pointer)
    );
    chat.value = '';
    canvas.focus({ preventScroll: true });
  }
});
const releaseInput = () => {
  runtime?._xp_game_clear_input();
  audio?.stop();
  updateAudio();
};
canvas.addEventListener('blur', releaseInput);
document.addEventListener('click', (event) => {
  // Clicking page content does not trigger the window's focus event.
  // Keep controls and their labels under their own focus handling.
  if (
    event.defaultPrevented ||
    !(event.target instanceof Element) ||
    event.target.closest(
      'input, textarea, select, button, a[href], label, [contenteditable], [tabindex]'
    )
  )
    return;
  focusGame();
});
window.addEventListener('blur', () => {
  windowFocused = false;
  releaseInput();
  audio?.mute(true);
});
window.addEventListener('focus', () => {
  windowFocused = true;
  audio?.mute(mute.checked || document.hidden);
  focusGame();
});
document.addEventListener('visibilitychange', () => {
  releaseInput();
  audio?.mute(mute.checked || document.hidden || !windowFocused);
});
window.addEventListener('resize', resize);
document.addEventListener('fullscreenchange', resize);
document.addEventListener('pointerlockchange', () => {
  if (!document.pointerLockElement) {
    runtime?._xp_game_pointer(0);
    releaseInput();
  }
});
canvas.addEventListener('webglcontextlost', (event) => {
  event.preventDefault();
  contextLost = true;
  releaseInput();
  runtime?._xp_game_context_lost();
  runtime?.xpilotSocket?.dispose();
  connection.textContent = 'Restoring graphics…';
});
canvas.addEventListener('webglcontextrestored', () => {
  if (runtime && runtime._xp_game_context_restored() !== 0) {
    stop('Graphics restoration failed', 'error');
    return;
  }
  contextLost = false;
  resize();
  advance();
});
nickname.addEventListener('change', persist);
mute.addEventListener('change', async () => {
  focusGame();
  audio?.mute(mute.checked || document.hidden);
  if (!mute.checked) await audio?.unlock();
  updateAudio();
  await persist();
});
document.querySelector('#save')!.addEventListener('click', async () => {
  focusGame();
  if (runtime) {
    if (runtime._xp_game_save_settings() !== 0)
      settingsStatus.textContent = 'Unable to save game settings';
  } else await persist();
});
document.querySelector('#mouse')!.addEventListener('click', () => {
  canvas.focus();
  runtime?._xp_game_pointer(document.pointerLockElement ? 0 : 1);
});
document.querySelector('#fullscreen')!.addEventListener('click', () => {
  canvas.focus();
  if (runtime?._xp_game_fullscreen(document.fullscreenElement ? 0 : 1) === -1)
    connection.textContent = 'Full screen request was declined';
});
window.addEventListener('pagehide', async (event) => {
  stop('Disconnected', 'disconnected');
  if (!event.persisted) await audio?.dispose();
});
try {
  try {
    preferences = await loadSettings();
  } catch (error) {
    settingsStatus.textContent = 'Settings unavailable: ' + String(error);
  }
  nickname.value = preferences.nickname;
  mute.checked = preferences.muted;
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
