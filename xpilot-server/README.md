# XPilot Infinity

Host an XPilot Infinity game with Node.js and play in a browser. The package
includes the WebAssembly server, browser client, maps, fonts, and other game
assets. Node.js 26.4 or later is required. No compiler or native addon is needed
to install or run the package.

Install a release archive and start the server:

```sh
npm install ./xpilot-infinity-VERSION.tgz
npx xpilot-infinity
```

Open `http://localhost:15347/` to play. The browser connects to the server that
provides the page. Native XPilot Infinity clients can join through UDP port
15345, TCP port 15346, or WebSocket port 15347. Press Ctrl+C to stop the server.

The server is controlled from the command line. It does not require a system
tray. Game data written by the server is kept in `.xpilot-infinity` under your
home directory by default. Change it with `--data-dir`:

```sh
npx xpilot-infinity --data-dir ./game-data \
  -listen udp:25345,tcp:25346,websocket:25347 \
  -rankFileName rankings.xml
```

Use `-map /path/to/map.xp2` for a custom map. Most options are the same as the
native server; `-help` displays their descriptions. Native boolean options use
`-option` to enable and `+option` to disable. Metaserver reporting is disabled
by default; `-reportMeta` enables it. The browser does not use a metaserver.

The npm edition shares its game and protocol implementation with the native
server. Debian packages and Windows installers also provide native server and
SDL client applications. See the [project documentation](https://github.com/kekyo/xpilot-infinity)
for game controls and native installation.

XPilot Infinity is distributed under the GNU General Public License. See
COPYING and the included dependency license notices.
