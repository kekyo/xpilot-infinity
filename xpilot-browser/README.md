# XPilot Infinity browser preview

This preview runs XPilot's C renderer in WebAssembly with SDL3 and WebGL 2. It displays shapes, an RGBA image, the bundled fonts, and browser font fallback. Game-server connections are the next implementation stage.

Use a desktop browser with WebGL 2, a keyboard, and a mouse. Click the scene and use the arrow keys to move the pink marker. Change the preview text or font to try multilingual text, and resize the window to resize the scene.

## Run the preview

From the repository root, with the submodules checked out:

```sh
npm --prefix xpilot-browser ci
npm --prefix xpilot-browser run dev
```

Open the local URL printed by Vite. The first run downloads Emscripten 5.0.1 and builds SDL3. On Linux, the build requires [Node.js 22.12 or later for Vite](https://vite.dev/guide/), npm, Git, Make, CMake, `flock`, and [Python 3 for Emscripten's tools](https://emscripten.org/docs/getting_started/downloads.html). Internet access is needed for the initial dependency downloads.

To create files for an HTTP server:

```sh
npm --prefix xpilot-browser run build
```

Serve the complete `xpilot-browser/dist/` directory over HTTP, including `runtime/` and `fonts/`. Opening `index.html` directly with a `file:` URL is unsupported. A secure public deployment will use HTTPS; server integration is still in progress.

The preview uses XPilot's bundled fonts first. The browser supplies missing characters from fonts available on your device, so multilingual text may look different across systems.

Development status and validation details are recorded in the [implementation plan](../doc/plans/plan3-web-native.md).
