// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import {
  createBrowserTextProvider,
  type BrowserTextProvider,
} from './browser-text';
import { loadBrowserRuntime, type BrowserRuntime } from './browser-runtime';
import './style.css';
import { createBrowserSocket, sameOriginEndpoint } from './browser-socket';

const canvas = document.querySelector<HTMLCanvasElement>('#game')!;
const viewport = document.querySelector<HTMLElement>('#viewport')!;
const status = document.querySelector<HTMLOutputElement>('#status')!;
const text = document.querySelector<HTMLInputElement>('#preview-text')!;
const face = document.querySelector<HTMLSelectElement>('#preview-font')!;
let runtime: BrowserRuntime | undefined;
let fonts: BrowserTextProvider | undefined;
let animation = 0;
let lastTime: number | undefined;
let lastScale = 0;
let disposed = false;
let suspended = false;
let joining = false;
const connection = document.querySelector<HTMLOutputElement>('#connection')!;
const connect = document.querySelector<HTMLButtonElement>('#connect')!;
const leave = () => {
  joining = false;
  runtime?._xp_browser_session_close();
  runtime?.xpilotSocket?.dispose();
  connection.textContent = 'Disconnected';
  connection.dataset.state = 'disconnected';
  connect.disabled = false;
};
document.querySelector('#disconnect')!.addEventListener('click', leave);
document.querySelector('#join')!.addEventListener('submit', (event) => {
  event.preventDefault();
  if (!runtime) return;
  leave();
  runtime.xpilotSocket = createBrowserSocket(
    new WebSocket(sameOriginEndpoint(location.href), 'xpilot-infinity-v1')
  );
  const bytes = new TextEncoder().encode(
    document.querySelector<HTMLInputElement>('#nickname')!.value + '\0'
  );
  const pointer = runtime._malloc(bytes.length);
  runtime.HEAPU8.set(bytes, pointer);
  const result = runtime._xp_browser_session_begin(pointer);
  runtime._free(pointer);
  joining = result === 0;
  connect.disabled = joining;
  connection.textContent = 'Connecting…';
  connection.dataset.state = 'connecting';
});

const stop = () => {
  disposed = true;
  cancelAnimationFrame(animation);
  observer.disconnect();
  text.removeEventListener('input', updateText);
  face.removeEventListener('change', updateText);
  runtime?._xp_browser_session_close();
  runtime?._xp_browser_shutdown();
  runtime = undefined;
  fonts?.dispose();
  fonts = undefined;
};

const uploadText = () => {
  if (runtime === undefined || fonts === undefined) return;
  const scale = Math.min(8, window.devicePixelRatio || 1);
  const lines = [
    { text: 'XPilot Infinity', face: 'mono' as const, size: 24, scale },
    {
      text: text.value,
      face: face.value === 'mono' ? ('mono' as const) : ('oblique' as const),
      size: 24,
      scale,
    },
  ];
  for (const [slot, request] of lines.entries()) {
    const raster = fonts.rasterize(request);
    const pointer = runtime._malloc(raster.pixels.byteLength);
    if (pointer === 0) throw new Error('Unable to allocate text pixels');
    try {
      runtime.HEAPU8.set(raster.pixels, pointer);
      if (
        runtime._xp_browser_upload_text(
          slot,
          pointer,
          raster.pixelWidth,
          raster.pixelHeight,
          raster.pixelWidth * 4,
          raster.pixelWidth / scale,
          raster.pixelHeight / scale,
          raster.offsetX,
          raster.offsetY
        ) !== 0
      )
        throw new Error('Unable to upload text');
    } finally {
      runtime._free(pointer);
    }
  }
  lastScale = scale;
};

const fail = (error: unknown) => {
  stop();
  status.textContent = `Rendering failed: ${error instanceof Error ? error.message : String(error)}`;
  status.dataset.state = 'error';
};

const render = (time: number) => {
  try {
    if (runtime === undefined || disposed || suspended) return;
    if (Math.min(8, window.devicePixelRatio || 1) !== lastScale) uploadText();
    if (joining) {
      const progress = runtime._xp_browser_session_step();
      if (progress !== 0) {
        joining = false;
        connect.disabled = false;
        connection.dataset.state = progress > 0 ? 'admitted' : 'error';
        connection.textContent =
          progress > 0
            ? 'Session admitted; initial data received'
            : runtime.UTF8ToString(runtime._xp_browser_session_error());
        if (progress < 0) runtime._xp_browser_session_close();
      }
    }
    const delta =
      lastTime === undefined ? 0 : Math.min((time - lastTime) / 1000, 0.05);
    lastTime = time;
    if (runtime._xp_browser_frame(delta) !== 0)
      throw new Error('The renderer could not draw a frame');
    canvas.dataset.sceneX = String(runtime._xp_browser_scene_x());
    canvas.dataset.sceneY = String(runtime._xp_browser_scene_y());
    animation = requestAnimationFrame(render);
  } catch (error) {
    fail(error);
  }
};

const dimensions = () => ({
  width: Math.max(128, Math.floor(viewport.clientWidth)),
  height: Math.max(128, Math.floor(viewport.clientHeight)),
});

const observer = new ResizeObserver(() => {
  if (runtime === undefined || fonts === undefined) return;
  const { width, height } = dimensions();
  if (runtime._xp_browser_resize(width, height) !== 0)
    fail(new Error('Unable to resize the canvas'));
});

const updateText = () => {
  try {
    uploadText();
  } catch (error) {
    fail(error);
  }
};

const start = async () => {
  try {
    const loadedRuntime = await loadBrowserRuntime(canvas);
    if (disposed) {
      loadedRuntime._xp_browser_shutdown();
      return;
    }
    runtime = loadedRuntime;
    const loadedFonts = await createBrowserTextProvider(
      new URL('fonts/VeraMoBd.ttf', document.baseURI).href,
      new URL('fonts/FreeSansBoldOblique.ttf', document.baseURI).href
    );
    if (disposed) {
      loadedFonts.dispose();
      return;
    }
    fonts = loadedFonts;
    const { width, height } = dimensions();
    if (runtime._xp_browser_init(width, height) !== 0)
      throw new Error('WebGL 2 initialization failed');
    uploadText();
    observer.observe(viewport);
    text.addEventListener('input', updateText);
    face.addEventListener('change', updateText);
    status.textContent = 'Rendering ready';
    status.dataset.state = 'ready';
    connect.disabled = false;
    if (!suspended) animation = requestAnimationFrame(render);
  } catch (error) {
    fail(error);
  }
};

window.addEventListener('pagehide', (event) => {
  cancelAnimationFrame(animation);
  lastTime = undefined;
  suspended = true;
  // A retained page owns its resources until it is discarded, not merely hidden.
  if (!event.persisted) stop();
});
window.addEventListener('pageshow', (event) => {
  if (!event.persisted || disposed) return;
  suspended = false;
  if (status.dataset.state === 'ready') {
    cancelAnimationFrame(animation);
    animation = requestAnimationFrame(render);
  }
});
await start();

// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/
