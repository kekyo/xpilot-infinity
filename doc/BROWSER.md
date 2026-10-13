# Browser hosting

The npm release includes a Node.js/WASM server and the browser application.
With Node.js 24.x (24.19.0+) or 26.4.0+, run `npx xpilot-infinity` and open
`http://localhost:15347/`. Alternatively, install it globally with
`npm install -g xpilot-infinity` and start it with `xpilot-infinity`. See the
[Node.js package guide](../xpilot-server/README.md) for options and saved data.

The [Podman image](../README.md#running-the-server-with-podman) bundles the same
Node.js server and browser client. Select the published transports with port
mappings; 15347/TCP is sufficient for browser hosting and WebSocket gameplay.

Native release packages also include the browser application. On Debian/Ubuntu:

```sh
xpilot-infinity-server -noQuit +reportMeta \
  -listen udp:15345,tcp:15346,websocket:15347 \
  -webRoot /usr/share/games/xpilot-infinity/browser
```

On Windows, from the installation directory:

```bat
xpilot-infinity-server.exe -noQuit +reportMeta -listen udp:15345,tcp:15346,websocket:15347 -webRoot lib/browser
```

Both commands serve the browser on `http://localhost:15347/`.

XPilot Infinity serves its browser client from the native server's WebSocket listener. Players open that address and join the same server. The [browser guide](../xpilot-browser/README.md) describes building the files, controls and saved settings.

## Server options

| Option | Meaning |
| --- | --- |
| `-webRoot PATH` | Serve regular files from this directory over GET and HEAD. `/` serves `index.html`. Omit this option to keep a WebSocket-only endpoint. |
| `-webOrigin ORIGIN` | Allow this exact browser origin, such as `https://game.example`. Without it, an Origin header must match `http://` followed by the request's Host. Use the browser's origin spelling, without a trailing slash. |
| `-webTrustedProxy ADDRESS` | Trust one numeric proxy address to provide `X-Real-IP`. Requires `webOrigin`. Every WebSocket request from that peer must contain one valid numeric client address. |

These options require a WebSocket listener and can also be specified in the server's defaults file. The static directory must be readable when the server starts. No directory listing is provided; dot/hidden path components, malformed escapes and access outside the configured root are rejected. On POSIX, path components cannot be symbolic links. On Windows, the opened file's resolved path must remain below the root and the final component cannot be a reparse point. Files above 64 MiB are rejected. HTTP headers are limited to 8 KiB, and pending HTTP connections and work per game frame are bounded.

The static server supplies the MIME types needed by HTML, JavaScript, CSS, WebAssembly, fonts, images and WAV/OGG effects. It closes each ordinary HTTP connection after its response. `/xpilot` upgrades to the existing `xpilot-infinity-v1` binary WebSocket protocol.

A native WebSocket client may omit Origin. The WebSocket endpoint permits public information queries and gameplay, but never grants local server administration based on loopback address and a claimed user name. Local administration remains available through the native TCP/UDP interfaces and their existing authorization rules.

## HTTPS and WSS

Put a TLS frontend in front of the native listener. Serve both the page and `/xpilot` at the same public origin; the browser automatically selects WSS for an HTTPS page. Start the backend on loopback, for example:

```sh
xpilot-infinity-server -listen websocket:15347 -serverHost 127.0.0.1 \
  -map ndh.xp2 -webRoot /srv/xpilot/browser \
  -webOrigin https://game.example -webTrustedProxy 127.0.0.1
```

Copy the browser build's complete `dist/` contents to `/srv/xpilot/browser`. A matching nginx server block is:

```nginx
server {
    listen 443 ssl;
    server_name game.example;
    ssl_certificate /etc/letsencrypt/live/game.example/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/game.example/privkey.pem;

    location / {
        proxy_pass http://127.0.0.1:15347;
        proxy_set_header Host $http_host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location = /xpilot {
        proxy_pass http://127.0.0.1:15347;
        proxy_http_version 1.1;
        proxy_set_header Host $http_host;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header X-Real-IP $remote_addr;
        proxy_read_timeout 90s;
    }
}
```

The frontend must overwrite `X-Real-IP` with the observed client address. XPilot ignores forwarded addresses from other peers. Trusted addresses are normalized before admission limits and reconnection checks; alternate IPv6 spellings do not create separate identities. A client cannot regain a session from a different effective address merely by claiming a forwarded header. Configure an explicit `webOrigin` when TLS terminates outside the game server.

nginx requires explicit Upgrade forwarding for [WebSocket proxying](https://nginx.org/en/docs/http/websocket.html). The backend can also omit `webRoot` when the frontend itself serves the complete browser directory. Keep the public origin and `/xpilot` location the same. The native SDL client's direct WebSocket target currently uses `ws://`; the browser provides WSS support through the TLS frontend.

## Assets and browser requirements

A desktop browser with WebGL 2 is required. The client uses a single WebAssembly instance for each joined session, with no pthread or COOP/COEP requirement. Audio and pointer lock follow browser user-gesture requirements; see [Web Audio autoplay](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API/Best_practices#autoplay_policy) and [Pointer Lock](https://developer.mozilla.org/en-US/docs/Web/API/Element/requestPointerLock).

Use a local filesystem for the static directory. Publish a complete browser build together so its JavaScript, WebAssembly and `.data` files match. The native server does not run Node.js or WebAssembly. It continues to support simultaneous UDP, TCP and WebSocket listeners.
