// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import type { BrowserRuntime } from './browser-runtime';
import type { BrowserTextProvider } from './browser-text';
import type { MapAssetFiles } from './map-assets';
import type { AudioFiles, BrowserAudio } from './browser-audio';

/** C game exports and browser services borrowed by one Emscripten instance. */
export interface GameRuntime extends Pick<
  BrowserRuntime,
  | 'HEAPU8'
  | '_malloc'
  | '_free'
  | 'UTF8ToString'
  | 'xpilotSocket'
  | '_xp_browser_session_begin'
  | '_xp_browser_session_step'
  | '_xp_browser_session_error'
  | '_xp_browser_session_close'
> {
  /** Public virtual filesystem API owned by this instance. */
  FS: MapAssetFiles & AudioFiles;
  /** Audio service borrowed from the page. */
  xpilotAudio: BrowserAudio | undefined;
  /** Current Unicode bitmap density. */
  xpilotTextScale: number;
  /** Notify the host after C saves its configuration file. */
  xpilotSettingsChanged: () => void;
  /** Request asynchronous departure when a C menu or key selects Quit. */
  xpilotQuit: () => void;
  /** Request browser pointer lock, reporting asynchronous denial to the UI. */
  xpilotPointerRequest: (enabled: boolean) => void;
  /** Number of logically held input actions. */
  _xp_game_held: () => number;
  /** Logical viewport width, independent of backing pixels. */
  _xp_game_width: () => number;
  /** Serialize native options and notify the page. */
  _xp_game_save_settings: () => number;
  /** Preserve CPU copies when the graphics context is lost. */
  _xp_game_context_lost: () => number;
  /** Recreate graphics objects after context restoration. */
  _xp_game_context_restored: () => number;
  /** Evict Unicode bitmaps after a display-scale change. */
  _xp_game_text_scale_changed: () => number;
  /** Request relative mouse steering from a user gesture. */
  _xp_game_pointer: (enabled: number) => number;
  /** Request full screen from a user gesture. */
  _xp_game_fullscreen: (enabled: number) => number;
  /** Complete map initialization after asynchronous asset installation. */
  _xp_game_assets_ready: () => number;
  /** Borrow the pending map's data URL as a UTF-8 pointer. */
  _xp_game_map_url: () => number;
  /** Borrow the latest received chat line as a UTF-8 pointer. */
  _xp_game_message: () => number;
  /** Server-confirmed ship heading. */
  _xp_game_heading: () => number;
  /** Cumulative number of shot particles received by this instance. */
  _xp_game_shots: () => number;
  /** Begin token resumption on the current replacement socket. */
  _xp_game_resume_begin: () => number;
  /** Advance token resumption: 0 pending, 1 resumed, -1 failure. */
  _xp_game_resume_step: () => number;
  /** Advance orderly quit: 0 pending, 1 finished. */
  _xp_game_quit_step: () => number;
  /** Clear held input without destroying the game. */
  _xp_game_clear_input: () => void;
  /** Canvas text provider, borrowed until shutdown. */
  xpilotText: BrowserTextProvider;
  /** Set up SDL and client options. Returns zero on success. */
  _xp_game_init: () => number;
  /** Adopt an admitted session. Returns zero on success. */
  _xp_game_begin: (nickname: number) => number;
  /** Advance once: 0 pending, 1 playing, 2 awaiting assets, -1 ended. */
  _xp_game_step: () => number;
  /** Release C client, SDL, and transport resources. */
  _xp_game_stop: () => void;
  /** Change logical canvas size. Returns zero on success. */
  _xp_game_resize: (width: number, height: number) => number;
  /** Send a null-terminated UTF-8 chat message. */
  _xp_game_chat: (address: number) => number;
  /** Last processed server frame. */
  _xp_game_frame: () => number;
  /** Current ship coordinate: axis zero x, one y. */
  _xp_game_position: (axis: number) => number;
}

/**
 * Load a fresh game instance with its read-only assets.
 * @param canvas SDL drawing and keyboard target.
 * @param fonts Loaded browser text service, borrowed by the instance.
 * @returns Runtime owned by the caller.
 */
export const loadGameRuntime = async (
  canvas: HTMLCanvasElement,
  fonts: BrowserTextProvider
): Promise<GameRuntime> => {
  const url = new URL('runtime/xpilot-game.mjs', document.baseURI);
  const module = (await import(/* @vite-ignore */ url.href)) as {
    default: (options: Record<string, unknown>) => Promise<GameRuntime>;
  };
  return await module.default({
    canvas,
    xpilotText: fonts,
    locateFile: (name: string) => new URL(name, url).href,
    print: (message: string) => console.info(message),
    printErr: (message: string) =>
      /ERROR:/.test(message) ? console.error(message) : console.info(message),
  });
};

/**
 * Borrow a null-terminated UTF-8 string to a synchronous C call.
 * @param runtime Instance receiving the allocation.
 * @param value Complete string, without embedded nulls.
 * @param action Synchronous C operation.
 * @returns The operation's return value. The allocation is always released.
 */
export const withGameString = (
  runtime: Pick<GameRuntime, '_malloc' | '_free' | 'HEAPU8'>,
  value: string,
  action: (address: number) => number
): number => {
  if (value.includes('\0')) throw new Error('Embedded null in text');
  const bytes = new TextEncoder().encode(value + '\0');
  const pointer = runtime._malloc(bytes.length);
  if (!pointer) throw new Error('Cannot allocate game text');
  try {
    runtime.HEAPU8.set(bytes, pointer);
    return action(pointer);
  } finally {
    runtime._free(pointer);
  }
};
