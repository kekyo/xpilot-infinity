import { expect, it } from 'vitest';
import { spawn } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { once } from 'node:events';
import { BoundSocket } from 'node:net';

it('serves the packaged browser and releases all listeners on CLI shutdown', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-cli-'));
  const reservations = [
    new BoundSocket(),
    new BoundSocket(),
    new BoundSocket(),
  ];
  const [udp, tcp, websocket] = reservations.map(
    (socket) => socket.address().port
  );
  for (const socket of reservations) socket.close();
  const server = spawn(process.execPath, [
    fileURLToPath(new URL('../dist/cli.mjs', import.meta.url)),
    '--data-dir',
    data,
    '-listen',
    `udp:${udp},tcp:${tcp},websocket:${websocket}`,
    '-serverHost',
    '127.0.0.1',
    '+reportMeta',
    '-noQuit',
  ]);
  let log = '';
  const ready = new Promise<void>((done, reject) => {
    const collect = (chunk: Buffer) => {
      log += chunk.toString();
      if (log.includes('Server runs at')) done();
    };
    server.stdout.on('data', collect);
    server.stderr.on('data', collect);
    server.once('error', reject);
    server.once('exit', (code) =>
      reject(new Error(`Server exited (${code}): ${log}`))
    );
  });
  try {
    await ready;
    const page = await fetch(`http://127.0.0.1:${websocket}/`);
    expect(page.status).toBe(200);
    expect(await page.text()).toContain('XPilot');
    const wasm = await fetch(
      `http://127.0.0.1:${websocket}/runtime/xpilot-game.wasm`
    );
    expect(wasm.status).toBe(200);
    expect(wasm.headers.get('content-type')).toContain('application/wasm');
    expect((await wasm.arrayBuffer()).byteLength).toBeGreaterThan(1000);
    const exited = once(server, 'exit');
    server.kill('SIGTERM');
    expect((await exited)[0], log).toBe(0);
    const rebound = new BoundSocket({ host: '127.0.0.1', port: websocket });
    rebound.close();
  } finally {
    if (server.exitCode === null && server.signalCode === null) {
      const exited = once(server, 'exit');
      server.kill('SIGTERM');
      await exited;
    }
    await rm(data, { recursive: true, force: true });
  }
});
