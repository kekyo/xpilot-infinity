#!/usr/bin/env node
import { mkdir } from 'node:fs/promises';
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
  for (let index = 2; index < process.argv.length; index++) {
    const arg = process.argv[index];
    if (arg === '--data-dir') {
      if (++index === process.argv.length)
        throw new Error('--data-dir requires a path.');
      data = resolve(process.argv[index]);
    } else if (arg === '--help') help = true;
    else args.push(arg);
  }
  if (help) {
    console.log(
      'Usage: xpilot-infinity [--data-dir PATH] [native server options]\n\n' +
        'Open http://localhost:15347/ to play. Stop with Ctrl+C.\n' +
        'Defaults: UDP 15345, TCP 15346, WebSocket/HTTP 15347.\n' +
        'Use -listen udp:PORT,tcp:PORT,websocket:PORT to change listeners.\n' +
        'Use -help to list native server options.'
    );
  } else {
    await mkdir(data, { recursive: true });
    const explicitTransport = args.some((arg) =>
      [
        '-listen',
        '-udp',
        '-tcp',
        '-websocket',
        '-port',
        '-gameTransport',
      ].includes(arg)
    );
    const defaults = ['-noQuit', '+reportMeta'];
    if (!explicitTransport)
      defaults.push('-listen', 'udp:15345,tcp:15346,websocket:15347');
    const listen = args.indexOf('-listen');
    const web =
      !explicitTransport ||
      (listen >= 0 && args[listen + 1]?.includes('websocket:')) ||
      args.includes('-websocket');
    if (web && !args.includes('-webRoot')) defaults.push('-webRoot', '/web');
    const server = await startServer(
      {
        runtime: new URL('./runtime/xpilot-server.mjs', import.meta.url),
        assets: fileURLToPath(new URL('./assets', import.meta.url)),
        web: fileURLToPath(new URL('./web', import.meta.url)),
        data,
        log: (line) => console.log(line),
      },
      [...defaults, ...args]
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
