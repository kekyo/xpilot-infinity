import { expect, it } from 'vitest';
import { spawn, type ChildProcess } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { once } from 'node:events';
import { BoundSocket } from 'node:net';

it.runIf(process.env.XPILOT_CLIENT_BINARY !== undefined)(
  'plays one WASM-hosted world with UDP, TCP, and WebSocket native clients',
  async () => {
    const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-clients-'));
    const reservations = [
      new BoundSocket(),
      new BoundSocket(),
      new BoundSocket(),
    ];
    const ports = reservations.map((socket) => socket.address().port);
    for (const socket of reservations) socket.close();
    const server = spawn(process.execPath, [
      process.env.XPILOT_NODE_SERVER_BINARY ??
        fileURLToPath(new URL('../dist/cli.mjs', import.meta.url)),
      '--data-dir',
      data,
      '-listen',
      `udp:${ports[0]},tcp:${ports[1]},websocket:${ports[2]}`,
      '-serverHost',
      '127.0.0.1',
      '+reportMeta',
      '-noQuit',
      '-minRobots',
      '0',
      '-maxRobots',
      '0',
      '-maxClientsPerIP',
      '8',
    ]);
    const clients: ChildProcess[] = [];
    const waitFor = (child: ChildProcess, marker: string): Promise<string> =>
      new Promise((done, reject) => {
        let log = '';
        const collect = (chunk: Buffer) => {
          log += chunk.toString();
          if (log.includes(marker)) done(log);
        };
        child.stdout!.on('data', collect);
        child.stderr!.on('data', collect);
        child.once('error', reject);
        child.once('exit', (code) =>
          reject(new Error(`Exited (${code}): ${log}`))
        );
      });
    try {
      await waitFor(server, 'Server runs at');
      for (const [index, transport] of ['udp', 'tcp', 'ws'].entries()) {
        const client = spawn(
          'xvfb-run',
          [
            '-a',
            'env',
            'LIBGL_ALWAYS_SOFTWARE=1',
            'SDL_VIDEODRIVER=x11',
            'stdbuf',
            '-oL',
            process.env.XPILOT_CLIENT_BINARY!,
            '-geometry',
            '800x600',
            '-join',
            '-name',
            `Node${transport}`,
            '-messagesToStdout',
            '2',
            `${transport}://127.0.0.1:${ports[index]}`,
          ],
          { detached: true }
        );
        clients.push(client);
        expect(await waitFor(client, 'Game frame ready')).toContain(
          'Game frame ready'
        );
      }
      for (const client of clients) expect(client.exitCode).toBeNull();
    } finally {
      for (const client of clients) {
        if (
          client.exitCode === null &&
          client.signalCode === null &&
          client.pid !== undefined
        ) {
          const exited = once(client, 'exit');
          process.kill(-client.pid, 'SIGTERM');
          await exited;
        }
      }
      if (server.exitCode === null && server.signalCode === null) {
        const exited = once(server, 'exit');
        server.kill('SIGTERM');
        await exited;
      }
      await rm(data, { recursive: true, force: true });
    }
  },
  60000
);
