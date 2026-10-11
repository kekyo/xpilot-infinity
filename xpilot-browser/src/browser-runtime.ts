// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** C renderer exports supplied by the generated Emscripten module. */
export interface BrowserRuntime {
  /** Current WASM byte view; reacquire after calls that may grow memory. */
  readonly HEAPU8: Uint8Array;
  /** Allocate byte storage; returns zero on failure. */
  readonly _malloc: (bytes: number) => number;
  /** Release storage previously allocated by the runtime. */
  readonly _free: (address: number) => void;
  /** Initialize SDL and WebGL using logical CSS dimensions. Returns zero on success. */
  readonly _xp_browser_init: (width: number, height: number) => number;
  /** Process input and draw one frame. Returns zero on success. */
  readonly _xp_browser_frame: (deltaSeconds: number) => number;
  /** Resize the logical viewport. Returns zero on success. */
  readonly _xp_browser_resize: (width: number, height: number) => number;
  /**
   * Copy an unpremultiplied text bitmap into a renderer texture.
   * @param slot Text line slot, zero or one.
   * @param address WASM address of borrowed RGBA bytes.
   * @param width Physical bitmap width.
   * @param height Physical bitmap height.
   * @param stride Bytes per row.
   * @param logicalWidth Bitmap width in CSS pixels.
   * @param logicalHeight Bitmap height in CSS pixels.
   * @param offsetX Bitmap left relative to the text origin.
   * @param offsetY Bitmap top relative to the alphabetic baseline.
   * @returns Zero on success; the caller can immediately free the borrowed bytes.
   */
  readonly _xp_browser_upload_text: (
    slot: number,
    address: number,
    width: number,
    height: number,
    stride: number,
    logicalWidth: number,
    logicalHeight: number,
    offsetX: number,
    offsetY: number
  ) => number;
  /** Release renderer resources, the context, and SDL. */
  readonly _xp_browser_shutdown: () => void;
  /** Current movable geometry X coordinate in CSS pixels. */
  readonly _xp_browser_scene_x: () => number;
  /** Current movable geometry Y coordinate in CSS pixels. */
  readonly _xp_browser_scene_y: () => number;
}

interface RuntimeOptions {
  readonly canvas: HTMLCanvasElement;
  readonly printErr: (message: string) => void;
}

/**
 * Instantiate the modularized runtime, including its adjacent WASM binary.
 * @param canvas Canvas borrowed by SDL for WebGL and input events.
 * @returns Initialized Emscripten exports; the caller owns their C lifecycle.
 */
export const loadBrowserRuntime = async (
  canvas: HTMLCanvasElement
): Promise<BrowserRuntime> => {
  const url = new URL('runtime/xpilot-browser.mjs', document.baseURI);
  const module = (await import(/* @vite-ignore */ url.href)) as {
    default: (options: RuntimeOptions) => Promise<BrowserRuntime>;
  };
  return await module.default({
    canvas,
    printErr: (message) => console.error(message),
  });
};
