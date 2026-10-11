#!/usr/bin/env node
import { mkdir, readFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { assertNodeRuntime } from './runtime';
import { startServer } from './server';

try {
  assertNodeRuntime();
  const args: string[] = [];
  let data = resolve(homedir(), '.xpilot-infinity');
  let help = false;
  let version = false;
  for (let index = 2; index < process.argv.length; index++) {
    const arg = process.argv[index];
    if (arg === '--data-dir') {
      if (++index === process.argv.length)
        throw new Error('--data-dir requires a path.');
      data = resolve(process.argv[index]);
    } else if (arg === '--help') help = true;
    else if (arg === '--version') version = true;
    else args.push(arg);
  }
  if (version) {
    // These assets are installed next to the bundle and resolved by Node.js.
    const info = JSON.parse(
      await readFile(
        new URL(/* @vite-ignore */ './build-info.json', import.meta.url),
        'utf8'
      )
    );
    console.log(`XPilot Infinity [${info.version}-${info.commit}]`);
  } else if (help) {
    console.log(
      'Usage: xpilot-infinity [--data-dir PATH] [native server options]\n\n' +
        'Open http://localhost:15347/ to play. Stop with Ctrl+C.\n' +
        'Defaults: UDP 15345, TCP 15346, WebSocket/HTTP 15347.\n' +
        'Use -listen udp:PORT,tcp:PORT,websocket:PORT to change listeners.\n' +
        'Use -help to list native server options.'
    );
  } else {
    await mkdir(data, { recursive: true });
    const server = await startServer(
      {
        runtime: new URL(
          /* @vite-ignore */ './runtime/xpilot-server.mjs',
          import.meta.url
        ),
        assets: fileURLToPath(
          new URL(/* @vite-ignore */ './assets', import.meta.url)
        ),
        web: fileURLToPath(
          new URL(/* @vite-ignore */ './web', import.meta.url)
        ),
        data,
        log: (line) => console.log(line),
      },
      args
    );
    process.once('SIGINT', server.stop);
    process.once('SIGTERM', server.stop);
    try {
      process.exitCode = await server.closed;
    } finally {
      process.removeListener('SIGINT', server.stop);
      process.removeListener('SIGTERM', server.stop);
    }
  }
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
}
