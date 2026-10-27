# XPilot Infinity browser client

Open a server's web address in a desktop browser with WebGL 2, a keyboard and a mouse. Enter a name and select Join game. The client joins the server that supplied the page; it does not use the metaserver or discover other servers.

Keyboard control starts when you join and returns after sending chat or using Mute or Save game settings. Click the game to resume keyboard control while editing chat or after focusing another part of the page. This does not lock the pointer; use Mouse steering for mouse control. The default keys are A/S to turn, Shift to thrust, Enter to fire and Space for shields. Chat accepts Unicode text, using XPilot's bundled fonts first and the browser's fonts for missing characters. Glyphs may look different across devices.

Mute controls effects. Sound starts after a user gesture. Mouse steering requests pointer lock; Escape releases it. Full screen requests the browser's full-screen mode. Losing focus releases held controls and stops effects. A short connection interruption is retried with the existing session; Leave game departs normally and allows another join.

The name and mute preference are saved for this site's origin. Save game settings also saves the game's native options; saving from the in-game configuration menu has the same effect. Settings are stored in IndexedDB and restored on reload. Clearing the site's data clears these settings. If storage is unavailable, the page reports that saving failed.

## Host a game

Build the browser files from this repository with its vendor submodules checked out:

```sh
npm --prefix xpilot-browser ci
npm --prefix xpilot-browser run build
```

The first build downloads Emscripten 5.0.1 and builds the bundled SDL3 dependencies. On Linux, it requires [Node.js 22.12 or later for Vite](https://vite.dev/guide/), npm, Git, Make, CMake, `flock`, and [Python 3 for Emscripten's tools](https://emscripten.org/docs/getting_started/downloads.html). Initial dependency downloads need internet access.

With the native XPilot Infinity server installed, run from the repository root:

```sh
xpilot-infinity-server -listen websocket:15347 -map ndh.xp2 \
  -webRoot "$PWD/xpilot-browser/dist"
```

Open `http://localhost:15347/`. Other machines can use this server's address. To offer native UDP and TCP as well, use `-listen udp:15345,tcp:15346,websocket:15347`. The browser still uses the WebSocket listener.

Keep the complete `dist/` directory together, including `runtime/` and `fonts/`. Opening the HTML through `file:` is unsupported. The game runs as WebAssembly with WebGL 2 and does not require cross-origin isolation or browser threads. A custom map's additional `.xpd` data belongs in `dist/maps/<basename>.xpd`; browser clients retrieve it only from the same origin, with an 8 MiB limit.

For public HTTPS/WSS deployment and proxy settings, see [browser hosting](../doc/BROWSER.md). TLS belongs at the frontend; the game server remains native.

## Local preview

To run through Vite, start a native WebSocket server and then:

```sh
XPILOT_SERVER_URL=ws://127.0.0.1:15347 npm --prefix xpilot-browser run dev
```

Open the URL printed by Vite. Its `/xpilot` proxy connects to the configured server. The separate `/preview.html` page displays the renderer and fonts without joining a game.

Implementation and validation records are in [plan3](../doc/plans/plan3-web-native.md).
