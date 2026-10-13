# XPilot Infinity

Multiplayer online space action since 1991.

![XPilot Infinity](./images/xpilot-infinity.png)

Copyright © 1991-2005 by Bjørn Stabell, Ken Ronny Schouten, Bert Gijsbers,
Dick Balaska, Uoti Urpala, Juha Lindström, Kristian Söderblom and Erik
Andersson.

See [COPYING](COPYING) for further details. You may not distribute this
project without all the documentation, source code, and the COPYING file.

> XPilot Infinity was renamed from XPilot NG.
> XPilot Infinity develops and distributes on [xpilot-infinity repository](https://github.com/kekyo/xpilot-infinity/) .

[![Project Status: Active – The project has reached a stable, usable state and is being actively developed.](https://www.repostatus.org/badges/latest/active.svg)](https://www.repostatus.org/#active)

---

[(Japanese language is here/日本語はこちら)](./README_ja.md)

## What is this?

[From www.xpilot.org](https://www.xpilot.org/about/):

XPilot is a multi-player 2D space game. Some features are borrowed from classics like the Atari coin-ups Asteroids and Gravitar, and the home-computer games Thrust (Commodore 64) and Gravity Force (Commodore Amiga), but XPilot has many new aspects too.

![XPilot](./images/game-client.png)

Highlights include:

- True client/server based game; optimal speed for every player.
- Meta server with up to date information about servers hosting games around the world.
- A web of world-wide rating servers; compare your skills with pilots from all around the world, and climb the ladder of the world-wide rating list.
- 'Real physics'; particles of explosions and sparks from your engines all affect you if you're hit by them. This makes it possible to kill someone by lowing them into a wall with engine thrust or shock waves from explosions.
- Specialized editors for editing ship-shapes and maps.
- Game objective and gameplay adjustable through a number of options, specified on the commandline, in special option files, or in the map files. Examples f modes of the game:
  - classical dogfight; equipped with only your gun, you have to rely on your maneuvering and tactical skills
  - team; fight together, steal other teams's treasures (involves flying around with a ball in a string, much like in Thrust) and blow up their targets (which are, no doubt, heavily guarded)
  - all out nuclear war; chose carefully between more than twenty weapon and defense systems to stay alive and annihilate your enemies
  - race; make it through the deadly course before your opponents 
- Adjustable gravity; adjustable by putting special attractors or deflectors in the world, or by adjusting the global gravity in various ways.
- Cannons and personalized and vengeful robot fighters give you a hard time.
- Watch your energy, and remember to 'dock' with a fuel station to refuel before it's too late.
- Defend your home base, or terrorize and steal someone else's.
- Equip your ship with the 15+ defense and weapon systems: afterburners, cloaking devices, sensors, transporters, extra cannons, mines and bombs, rockets smarts, torpedos and nuclear), ECM, laser, extra tanks, autopilot etc.

## Installation and documentation

Available binary packages in [xpilot-infinity repository](https://github.com/kekyo/xpilot-infinity/releases/) .

The supported package matrix is:

| Distribution | Release | Architectures |
| :--- | :--- | :--- |
| Debian | bookworm | amd64, i386, arm64, armhf |
| Debian | trixie | amd64, i386, arm64, armhf, riscv64 |
| Ubuntu | 22.04 | amd64, arm64 |
| Ubuntu | 24.04 | amd64, arm64 |
| Ubuntu | 26.04 | amd64, arm64 |
| Windows | - | x86 (32bit), x86_64 (64bit) |
| Node.js / npm | 24.x (24.19.0+) or 26.4.0+ | Platforms supported by Node.js |

- Environment: SDL3, OpenGL 3.3 and OpenAL (sound)

The Debian and Ubuntu packages add **XPilot Infinity** to XDG-compatible
application menus. The launcher uses the packaged application icon and opens
the SDL server browser without additional arguments.

### Play on Node.js and browser

Play using the native client or a browser. See [Playing in a browser](docs/en/browser.md)
for joining a game, controls, and starting your own server.

The [Node.js package](xpilot-server/README.md)
includes a WASM server and browser client. It requires Node.js 24.x (24.19.0+)
or 26.4.0+.
See [browser hosting](doc/BROWSER.md) for native hosting and HTTPS/WSS deployment.

Install the npm package globally and start the server:

```sh
npm install -g xpilot-infinity
xpilot-infinity
```

Alternatively, start it with npx:

```sh
npx xpilot-infinity
```

Open `http://localhost:15347/`. UDP 15345 and TCP 15346 are also available for
native clients. Stop the server with Ctrl+C. This package runs with Node.js and
a browser; it needs no native compiler or separate native server. Settings and
saved data belong in `--data-dir`, which defaults to `~/.xpilot-infinity`.

For instructions on how to join, basic operations, and how to start your own server, see [Play in a Browser](docs/en/browser.md).

### Play with a Docker container

Use Docker or Podman to start an image containing the server and browser
client. Linux amd64 and arm64 are supported.

```sh
docker run --rm -p 15347:15347/tcp docker.io/kekyo/xpilot-infinity:latest
```

Open `http://localhost:15347/`, enter a name, and choose `Join game`.
Stop the server with Ctrl+C. For Podman, replace `docker` with `podman`.

This example publishes only browser hosting and WebSocket gameplay.
For saved data, TCP/UDP access, and building your own image, see
[Running the server with Podman](#running-the-server-with-podman) below.

### Windows installation

Windows releases provide both an NSIS installer and a ZIP archive for each
architecture. For a normal installation, run the matching
`xpilot-infinity-<version>-windows-<architecture>-setup.exe`. It installs the
client, server, game data, and Start menu shortcut; selecting **XPilot
Infinity** from the Start menu opens the SDL server browser.

The ZIP archive remains the portable distribution. Extract it to any writable
directory and run `xpilot-infinity-sdl.exe` directly; installing the NSIS
package is not required for portable use.

Or, installation from source code instructions are in [INSTALL](INSTALL).

## Quick start the game

XPilot consists of a server program and a client program.
It is a multiplayer game in which multiple clients can connect to a single server.

First, start the server:

```bash
xpilot-infinity-server -noQuit
```

`-noQuit` is a parameter that keeps the server running even if all clients disconnect.
The same server process remains available.
To shut down the server, simply press Ctrl-C or similar.

While the server is running, just type it to show server list both local network and meta-server:

```bash
xpilot-infinity-sdl
```

![Server list](./images/server-list.png)

> A meta-server is a server that manages a list of XPilot servers available worldwide. It is operated by xpilot.org.

You can connect to the server and start a game.

Or, connect server directly with the URL like:

```bash
xpilot-infinity-sdl udp://localhost
```

As indicated by the `udp://` URL, the UDP protocol is used (By default, UDP is compatible with the latest version of XPilot NG).
You can also use TCP (`tcp://`) or WebSocket (`ws://`).

If you launch the SDL client without specifying a URL, it will query the meta-server and perform one UDP discovery pass on the local network.
The server browser combines both result sets, identifies their source, and places LAN servers first.

Since the server accepts connections using the UDP protocol by default, it should be automatically listed in this procedure.
You can disable the LAN query with `-localDiscovery no`.

There are still plenty of other features, but I can't cover them all here.

The documentation for XPilot Infinity is far from complete. For further reading, see the manuals in the [`doc/man`](doc/man) directory.

---

## Differences between XPilot NG and XPilot Infinity

XPilot Infinity's goal is to ensure that XPilot runs smoothly even in modern environments.
The main changes from NG version 4.7.3 are as follows:

- Added network protocols: TCP and WebSocket. This may make it easier to bypass routers and firewalls.
- Update to a modern graphics environment: Replaced with SDL3 and supported OpenGL 3.3 core profile.
- The internal structure has been slightly refactored.
  This was also essential to achieving the changes mentioned above.
  However, UDP protocol compatibility with version 4.7.3 has been maintained.
- Support for multilingual fonts via TTF in user name.
  For now, you should just install [Noto Sans Mono and families](https://fonts.google.com/noto/specimen/Noto+Sans+Mono).
- Sound engine is enabled.
- The Windows binaries have been modified to build using the MinGW toolchain.
- Debian packages (*.deb) can now be built for several architectures (using podman).

## Network transport

One server can accept UDP, TCP, and WebSocket players in the same game:

```console
xpilot-infinity-server -listen udp:15345,tcp:15346,websocket:15347 -map ndh.xp2
xpilot-infinity-sdl -join udp://server.example:15345
xpilot-infinity-sdl -join tcp://server.example:15346
xpilot-infinity-sdl -join ws://server.example:15347
```

`listen` accepts any nonempty combination of these transports, once each,
with ports from 1 to 65535. `ws` is an alias for `websocket`; transport names
are case-insensitive. TCP and WebSocket require separate ports. UDP may use
the same number as a stream listener when metaserver reporting is disabled
with `+reportMeta`. With reporting enabled, every listener requires a distinct
port and appears separately in the server list, with the same players and map.
Player limits, names, and per-IP limits are shared across all listeners.

The setting is also available as `listen: udp:15345,tcp:15346,websocket:15347`
in the server defaults file. It cannot be combined with explicitly configured
`contactPort`/`port`, `contactTransport`, `gameTransport`, or transport shortcuts,
including settings in defaults files. Restart the server to change listeners.
Invalid settings or an unavailable listener port fail the entire startup.
Without `listen`, the single-listener settings below apply; the default is UDP
on port 15345.

For the example above, publish UDP port 15345 and TCP ports 15346 and 15347 in
the firewall or container configuration. UDP gameplay also requires a port
per player in the server's `clientPortStart`/`clientPortEnd` range. Fixed TCP
and WebSocket sessions use their listener port for both admission and gameplay.
Metaserver reporting sends UDP from each listener's numeric port, including
TCP and WebSocket listeners. Reconnection uses the original protocol and port;
it does not move a player between transports.

Contact/lobby and gameplay connections use UDP by default and can be selected
independently. To use TCP for both, start both endpoints with matching
settings, for example:

```console
xpilot-infinity-server -transport tcp [options]
xpilot-infinity-sdl -join tcp://server.example
```

Or use WebSocket for both:

```console
xpilot-infinity-server -websocket [options]
xpilot-infinity-sdl -join ws://server.example
```

The server shortcuts `-tcp`, `-udp`, `-websocket`, and
`-transport udp|tcp|websocket` select one transport for both contact/lobby and
gameplay. Command-line transport options are applied from left to right, so a
later `contactTransport` or `gameTransport` option can still create a split
UDP/TCP configuration. WebSocket must be selected for both transports.

Direct client targets accept `HOST`, `ws://HOST[:PORT]`,
`tcp://HOST[:PORT]`, or `udp://HOST[:PORT]`. A qualified target selects that
transport for both contact/lobby and gameplay, while a bare host uses the
`contactTransport`, `gameTransport`, and `port` option defaults. An explicit
target port overrides `-port`. Split UDP/TCP combinations remain available
through the long options or metaserver advertisements.

WebSocket uses RFC 6455 binary messages at `/xpilot` with the
`xpilot-infinity-v1` subprotocol; each message carries one logical XPilot
record. Native clients support direct `ws://` connections. The browser client
uses the same-origin WebSocket endpoint, including `wss://` through a TLS
frontend. Use `-webRoot PATH` to serve the browser build from this listener;
see [browser hosting](doc/BROWSER.md) for Origin and trusted-proxy settings.
WebSocket connections cannot claim loopback-based server administration.

Multiple targets are tried in command-line order, for example
`ws://server1 tcp://server2 udp://server3`. This is explicit fallback between
targets; the client does not automatically probe multiple protocols for one
target. Target hosts are DNS names or IPv4 addresses. IPv6 literals and
general URI features such as credentials, paths, queries, fragments, and
percent encoding are not supported. TCP and WebSocket contact require an
explicit target or a metaserver entry because LAN broadcast discovery is
available only with UDP. With no explicit target, the SDL server browser makes
one UDP LAN discovery pass alongside its metaserver query. LAN and metaserver
results remain usable if the other query fails, and matching endpoint and
transport pairs are shown as one `LAN + Meta` entry. Use
`-localDiscovery no` to disable the LAN query.

If every explicit target fails to return a valid contact response after its
retries, the SDL client writes a final summary containing the last endpoint
and both selected transports to standard error. A normal graphical invocation
also displays the summary in one modal error dialog and exits unsuccessfully
after it is acknowledged. An explicit `-text` or `-list` invocation reports
through the terminal without opening that dialog. A server response used to
complete `-list`, or a later target that responds successfully, is not reported
as a connection failure.

Metaserver advertisements include both transport selections. The SDL and X11
clients apply the advertised values automatically when a server is selected;
legacy advertisements without transport metadata are treated as UDP contact
and UDP gameplay servers. Metaserver query and reporting traffic continues to
use its existing protocols.

The server's `clientPortStart` and `clientPortEnd` range applies to traditional
UDP and split UDP/TCP gameplay connections. On clients it also applies to gameplay and the
traditional UDP contact socket; fixed TCP and WebSocket sessions use that
range for their local source port.

TCP gameplay records contain a two-byte network-order payload length followed
by an unchanged XPilot packet payload. Server recordings must be replayed with
the same `gameTransport` value that was used while recording. Server recording
and playback are unavailable when fixed TCP or WebSocket sessions are enabled;
the server rejects those combinations, including attempts to enable recording
while it is running. Traditional UDP and split UDP/TCP recording remain available.

### Running the server as a systemd service

The Debian and Ubuntu packages install an optional
`xpilot-infinity-server.service`. Installing the package does not start or
enable the service. To start it now and on subsequent boots, run:

```bash
sudo systemctl enable --now xpilot-infinity-server.service
```

The packaged service runs the `ndh.xp2` map continuously and does not advertise
itself to the public metaserver. Its default command-line options are stored in
`/etc/default/xpilot-infinity-server`. Edit `XPILOT_SERVER_OPTIONS` there to
select another map or configure other server options, then restart the service:

```bash
sudo systemctl restart xpilot-infinity-server.service
```

The configuration file uses systemd `EnvironmentFile` syntax rather than shell
syntax. Replace `+reportMeta` with `-reportMeta` only when the server should be
advertised publicly. Relative output files such as recordings are written
below `/var/lib/xpilot-infinity-server`.

Check the current status and follow the server log with:

```bash
systemctl status xpilot-infinity-server.service
journalctl -u xpilot-infinity-server.service -f
```

To stop the server and prevent it from starting on future boots, run:

```bash
sudo systemctl disable --now xpilot-infinity-server.service
```

### Running the server as a Windows service

On the installer's component page, optionally select **Dedicated server
service (manual start)**. The component is not selected for a new
installation. When selected, it registers `XPilotInfinityServer` with startup
type **Manual** under the `LocalService` account, but the installer does not
start it. Installing with the default component selection does not register a
service.

Open an elevated Command Prompt or PowerShell window to start and stop the
registered server manually:

```bat
sc.exe start XPilotInfinityServer
sc.exe stop XPilotInfinityServer
```

To enable startup on subsequent Windows boots, change its startup type to
automatic. This command alone does not start it in the current session:

```bat
sc.exe config XPilotInfinityServer start= auto
sc.exe start XPilotInfinityServer
```

Return it to manual startup with:

```bat
sc.exe stop XPilotInfinityServer
sc.exe config XPilotInfinityServer start= demand
```

The server configuration and log are stored under
`%ProgramData%\XPilot Infinity\server`. The default configuration runs
`ndh.xp2` continuously without advertising it to the public metaserver. Edit
`xpilot-infinity-server.conf` before starting the service to change those
settings. Upgrades and uninstallation preserve this directory; the
uninstaller stops and removes the service itself.

For an unattended installation, `/S` selects the normal client/server files
only. Add `/SERVER_SERVICE=1` to register the optional service. If `/D` is used
to override the install directory, NSIS requires it to be the final argument:

```bat
xpilot-infinity-setup.exe /S /SERVER_SERVICE=1 /D=C:\Games\XPilotInfinity
```

### Controlling the server from the desktop

Launch XPilot Infinity Server Tray from the application menu, or run
`xpilot-infinity-tray`. It runs as your normal desktop user and connects to
an existing local service. On Linux, install the Debian package containing
`xpilot-infinity-server.service`. On Windows, select the optional server
service component in the installer. The tray does not register a service or
start it automatically. A ZIP copy of the tray can connect to a registered
service; install the matching server and authorization helper in a protected
installation directory first.

Use Start server and Stop server to control the service. Administrative
approval through polkit (Linux) or UAC (Windows) is requested when needed.
Selecting a map changes the shared server configuration; changing it while
the server is running restarts the service and disconnects players. The
configured map shown in the menu does not certify which map a running game
has loaded. Installed maps are found independently of the working directory.

Edit configuration opens a private copy in your default plain-text editor.
Save the copy as UTF-8 without a byte-order mark, then choose Apply saved
changes in the tray and confirm the operation. Saving in the editor alone
does not change the service. Applying while stopped only saves settings;
applying while running also restarts the service. The editor runs with your
normal permissions. Free configuration editing requires separate approval
from map selection.

Unsaved editor changes are not applied. Saved, unapplied changes survive tray
restarts: reopen the copy to continue or choose Discard editing copy to start
over. If another session changes shared settings, applying the old copy
reports a conflict and preserves your work. Copy any changes you want to keep
before discarding it. Authentication cancellation and failed saves also keep
the editing copy. A successful save followed by a failed restart is reported
separately; use the details action and service logs to investigate.

Shared settings are stored in `/etc/default/xpilot-infinity-server` on Linux
and `%ProgramData%\XPilot Infinity\server\xpilot-infinity-server.conf` on
Windows. Linux logs are available through
`journalctl -u xpilot-infinity-server.service`; Windows service logs are in
`%ProgramData%\XPilot Infinity\server`. The details action shows the paths
used by your installation. The tray controls the system service, so the
separate Podman user service described below is outside its scope.

On X11 and Wayland, a compatible StatusNotifierItem host displays the icon;
X11 also supports XEmbed trays. If no tray host is available, a control
window provides the same operations. Exiting the tray, closing that window,
or logging out leaves the server running. Choose Stop server to shut it down.

### Running the server with Podman

The image includes the Node.js/Wasm server, standard maps, and browser client.
It supports Linux amd64 and arm64 and runs as UID/GID `10001:10001`.
UDP, TCP, and WebSocket are all enabled inside the container. Choose which
transports to publish through the port mappings at startup.

#### Run in the background and save data

The detailed examples below use Podman. If the quick-start container is still
running, stop it with Ctrl+C before switching. A named volume preserves rankings
when you recreate the container.

```bash
image=docker.io/kekyo/xpilot-infinity:latest
podman volume create xpilot-infinity-server-data
podman run --detach \
  --name xpilot-infinity-server \
  --read-only --cap-drop=all \
  --security-opt=no-new-privileges \
  --publish 15347:15347/tcp \
  --volume xpilot-infinity-server-data:/var/lib/xpilot-infinity-server \
  "$image" -map ndh.xp2 -rankFileName rankings.xml
```

This example publishes browser hosting and WebSocket gameplay. Both use
15347/TCP, so static files need no separate port. For local access only, use
`--publish 127.0.0.1:15347:15347/tcp`.
The default map is `ndh.xp2`, and public metaserver reporting is disabled.
Players on another PC replace `localhost` in the browser URL with the server's hostname.

#### Choose the published transports

Create the container with the desired combination of `--publish` options.
No server argument changes or image rebuilds are required.

| Purpose | Container port | Podman option |
| --- | --- | --- |
| Browser hosting and WebSocket | 15347/TCP | `--publish 15347:15347/tcp` |
| Native TCP | 15346/TCP | `--publish 15346:15346/tcp` |
| Native UDP contact | 15345/UDP | `--publish 15345:15345/udp` |
| Native UDP gameplay | 15400–15499/UDP | `--publish 15400-15499:15400-15499/udp` |

UDP requires both the contact port and gameplay range. Clients use the gameplay
port number reported by the server, so keep 15400–15499 identical on the host.
The image's `EXPOSE` metadata lists only 15347/TCP; publish UDP explicitly.

For example, publish all transports with:

```bash
podman run --detach --name xpilot-infinity-server-all \
  --read-only --cap-drop=all --security-opt=no-new-privileges \
  --publish 15347:15347/tcp \
  --publish 15346:15346/tcp \
  --publish 15345:15345/udp \
  --publish 15400-15499:15400-15499/udp \
  "$image"
```

Stop the first example's container before switching, since both use host port
15347. Native clients can join `tcp://server.example:15346`,
`udp://server.example:15345`, or `ws://server.example:15347`.
Do not use `--network=host` when choosing exposure through port mappings.

Whether rootless forwarding preserves client IP addresses depends on its
network settings. Check the addresses visible to the server before configuring
per-IP player limits. See the [Podman run documentation](https://docs.podman.io/en/v4.9.3/markdown/podman-run.1.html)
for port publishing, source IPs, and SELinux mount options.

#### Maps, saved data, and HTTPS

Adding arguments such as `-map` after the image name retains all three listeners
and the UDP gameplay range. Select bundled maps by basename, or mount custom
maps and defaults files read-only:

```bash
podman run --detach --name xpilot-infinity-server-custom \
  --read-only --cap-drop=all --security-opt=no-new-privileges \
  --publish 15347:15347/tcp \
  --volume "$PWD/defaults.txt:/etc/xpilot/defaults.txt:ro" \
  --volume "$PWD/my-map.xp2:/maps/my-map.xp2:ro" \
  --volume xpilot-infinity-server-data:/var/lib/xpilot-infinity-server \
  "$image" -defaultsFileName /etc/xpilot/defaults.txt \
  -map /maps/my-map.xp2 -rankFileName rankings.xml
```

Saved data goes into `/var/lib/xpilot-infinity-server`. Mount a named volume to
retain rankings and other output. Run only one server per data volume at a time.
On SELinux hosts, add an appropriate bind mount label, such as `:ro,Z`.

Keep the operator password in an external file using the format in
[`lib/password.txt`](lib/password.txt). Import it with
`podman secret create xpilot-server-password ./password.txt`, then add
`--secret source=xpilot-server-password,target=xpilot-password,uid=10001,gid=10001,mode=0400`
before the image name and `-passwordFileName /run/secrets/xpilot-password` after it.

For HTTPS, put a TLS reverse proxy in front of the container. Publish the backend
with `--publish 127.0.0.1:15347:15347/tcp` and add the server argument
`-webOrigin https://game.example`. Forward both the page and `/xpilot` at that
same origin; the browser selects WSS automatically.
See the [proxy configuration example](doc/BROWSER.md#https-and-wss).
If using `-webTrustedProxy`, specify the proxy's actual peer IP as seen inside
the container.

```bash
podman logs --follow xpilot-infinity-server
podman stop --time 60 xpilot-infinity-server
```

`podman stop` sends SIGTERM and allows the server to save its output.

#### Manage with rootless systemd

The supplied Quadlet publishes only 15347/TCP and saves rankings in a named
volume. Its default `Image=` is `docker.io/kekyo/xpilot-infinity:latest`.
For a local build, change `Image=` in the copied file to your local tag.

```bash
quadlet_dir="${XDG_CONFIG_HOME:-$HOME/.config}/containers/systemd"
mkdir -p "$quadlet_dir"
cp containers/xpilot-infinity-server.container "$quadlet_dir/"
cp containers/xpilot-infinity-server-data.volume "$quadlet_dir/"
# For local builds, set Image= in the copied .container file.
systemctl --user daemon-reload
systemctl --user start xpilot-infinity-server.service
```

Stop any manually started container using the same host ports first.
Inspect the service with `systemctl --user status xpilot-infinity-server.service`
and `journalctl --user -u xpilot-infinity-server.service`.
To publish TCP or UDP as well, add corresponding `PublishPort=` entries from the
table above to `[Container]`. Once a secret exists, add `Secret=` and append
`-passwordFileName` to `Exec=`.

The `[Install]` section starts the service with subsequent user systemd sessions;
there is no need to run `systemctl enable` on the generated service.
See the [Quadlet documentation](https://docs.podman.io/en/v4.9.3/markdown/podman-systemd.unit.5.html)
for startup without an interactive login. Automatic image updates are disabled;
pull the replacement image before restarting the service.

#### Build your own image

Install Podman, Node.js 24.x (24.19.0+) or 26.4.0+, and npm, together with the
[browser build prerequisites](xpilot-browser/README.md).
Building and verifying a foreign CPU image also requires registered QEMU/binfmt
support, such as the host OS's `qemu-user-static` package.

Artifact versions and commit IDs are derived from Git using `screw-up`.
The build scripts run `npx --yes screw-up format -f`, so a separate `screw-up`
installation is not required. Run `./build_container_image.sh --print-version`
to check the version without building.

```bash
image=localhost/xpilot-infinity:local
./build_container_image.sh --tag "$image"
```

To use the resulting local image, set `image` to this tag in the startup examples
above.

By default, the build creates an amd64/arm64 manifest and individual verification
tags `${image}-amd64` and `${image}-arm64`. The server and browser are built once,
with the same version and commit used for both CPU images. Both images must pass
startup, WebSocket, browser hosting, and persistence checks before the final tag
is updated. A build or verification failure prevents that update and publishing.

To build one CPU only, pass `--platform linux/amd64` or `--platform linux/arm64`.
Use `--jobs 1` for sequential image builds. `--skip-app-build` reuses existing
server and browser artifacts only if their versions and commits match the
requested build identity. Omit it after changing application sources.

`./build_container_image_all.sh` also targets both CPUs. Builds remain local
unless `--push` is supplied; that option publishes both the version and `latest`
tags after verification. The default repository is
`docker.io/kekyo/xpilot-infinity`. Set your own repository with
`--registry-image` and authenticate with `podman login` beforehand.
Use `--version`, `--revision`, and `--node-image` to override the product version,
source commit, and Node.js base image. Run `./build_container_image.sh --help`
for all options.

---

## Other sources of information

- [XPilot NG](http://xpilot.sourceforge.net/)
- XPilot FAQ: `telnet meta.xpilot.org 4402` (also in the `doc` directory)
- [XPilot website](http://www.xpilot.org/)
- [XPilot for Windows](http://www.buckosoft.com/xpilot/)
- [XPilot Newbie Manual](http://bau2.uibk.ac.at/erwin/NM/www)
- [XPilot FTP archive](ftp://ftp.xpilot.org/pub/)
- XPilot newsgroup: `rec.games.computer.xpilot`

## Contributed software

Check the `contrib` directory for extra programs.

Also try the included xp2 map editor. It is a graphical editor with which you
can design a new XPilot world.

## License

Under GPLv2.
