# Playing in a browser

[日本語](../ja/browser.md) · [Back to README](../../README.md)

Use a desktop browser with WebGL 2, a keyboard, and a mouse.
You can join a game without installing a client.

## Join a game

1. Open the URL provided by the server operator. If you start the npm server on your own PC as described below, use `http://localhost:15347/`.
2. When `Ready to join` appears, enter your player name in `Name` and select `Join game`.
3. When `Playing` appears, click the game to focus its controls.
4. Select `Leave game` to leave. Select `Join game` again to rejoin.

The browser connects to the server that supplied the page. It does not show a metaserver list or search for other servers.

## Basic controls

The default keys are:

| Action | Key |
| --- | --- |
| Turn | A / S |
| Thrust | Shift |
| Fire | Enter |
| Shield | Space |

To chat, type in the `Chat` field below the game and select `Send`. Click the game afterward to resume flying.

- `Full screen`: Show the game in full screen. Press Esc to exit.
- `Mouse steering`: Steer with the mouse. Press Esc to release the pointer.
- `Mute`: Mute sound effects.
- `Save game settings`: Save changes made in the game's configuration.

Your player name and mute preference are saved automatically. Settings are stored in your browser for each site and restored when you return.
If the keys do not respond, click the game to focus it.

## Start a server on your PC

You need Node.js 26.4 or later, npm, and the distributed `xpilot-infinity-VERSION.tgz` archive. Run these commands in the directory containing the archive, replacing `VERSION` with the actual filename's version:

```sh
npm install ./xpilot-infinity-VERSION.tgz
npx xpilot-infinity
```

Open `http://localhost:15347/` and join as described above. To stop the server, press Ctrl+C in the terminal where you started it.
The package includes the server and browser client; no C compiler or separate native server is needed to run it.

From another PC on the same network, replace `localhost` with the server's hostname or IP address. The browser must be able to reach TCP port 15347 on the server.

See the [Node.js package guide](../../xpilot-server/README.md) for server options, or [browser hosting](../../doc/BROWSER.md) for native hosting and public HTTPS deployment.

Open the server's `http://` or `https://` URL rather than opening the HTML file directly.
