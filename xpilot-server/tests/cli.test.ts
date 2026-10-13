import { expect, it } from 'vitest';
import { execFile, spawn } from 'node:child_process';
import { promisify } from 'node:util';
import { cp, mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { once } from 'node:events';
import { BoundSocket } from 'node:net';

it.each(['command line', 'defaults file'])(
  'serves the selected browser and releases listeners configured by %s',
  async (configuration) => {
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
    const listen = `udp:${udp},tcp:${tcp},websocket:${websocket}`;
    const config = resolve(data, 'server.txt');
    const customWeb = resolve(data, 'public');
    await mkdir(resolve(customWeb, 'runtime'), { recursive: true });
    await writeFile(
      resolve(customWeb, 'index.html'),
      'XPilot configured browser'
    );
    await cp(
      new URL('../dist/web/runtime/xpilot-game.wasm', import.meta.url),
      resolve(customWeb, 'runtime/xpilot-game.wasm')
    );
    await writeFile(config, `listen: ${listen}\nwebRoot: ${customWeb}\n`);
    const args =
      configuration === 'command line'
        ? ['-listen', listen]
        : ['-defaults', config];
    const server = spawn(process.execPath, [
      fileURLToPath(new URL('../dist/cli.mjs', import.meta.url)),
      '--data-dir',
      data,
      ...args,
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
      expect(await page.text()).toContain(
        configuration === 'command line'
          ? 'XPilot'
          : 'XPilot configured browser'
      );
      const wasm = await fetch(
        `http://127.0.0.1:${websocket}/runtime/xpilot-game.wasm`
      );
      expect(wasm.status).toBe(200);
      expect(wasm.headers.get('content-type')).toContain('application/wasm');
      expect((await wasm.arrayBuffer()).byteLength).toBeGreaterThan(1000);
      if (process.env.XPILOT_CONTACT_TARGET_PROBE !== undefined) {
        // Reserve a gameplay socket but leave before creating a player. Shutdown
        // must close this pending login as well as established players.
        const target = `udp://127.0.0.1:${udp}`;
        await promisify(execFile)(process.env.XPILOT_CONTACT_TARGET_PROBE, [
          target,
          target,
        ]);
      }
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
  }
);
