# XPilot Infinity

Host an XPilot Infinity game with Node.js and play in a browser. The package
includes the WebAssembly server, browser client, maps, fonts, and other game
assets. Node.js 24.x (24.19.0+) or 26.4.0+ is required. No compiler or native
addon is needed to install or run the package.

Install the npm package globally and start the server:

```sh
npm install -g xpilot-infinity
xpilot-infinity
```

Alternatively, start it with npx:

```sh
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

Gzip maps (`.xp2.gz`) are supported without an external decompressor. Use
`-defaults /absolute/path/server.txt` for a configuration file. Absolute paths
in options and configuration files refer to your operating system; relative
paths refer to `--data-dir`. Packaged maps can also be selected by name, for
example `-map ndh.xp2`. Rankings and recordings are saved outside the installed
package. UDP games support the native `-recordFileName` and `-recordMode`
record/replay options; recording with TCP or WebSocket gameplay is unsupported.

The npm edition shares its game and protocol implementation with the native
server. Debian packages and Windows installers also provide native server and
SDL client applications. See the [project documentation](https://github.com/kekyo/xpilot-infinity)
for game controls and native installation.

XPilot Infinity is distributed under the GNU General Public License. See
COPYING and the included dependency license notices.
