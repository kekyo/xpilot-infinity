// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn, type ChildProcess } from 'node:child_process';
import { once } from 'node:events';
import { mkdtemp, mkdir, writeFile, symlink, rm, open } from 'node:fs/promises';
import { createServer, connect } from 'node:net';
import { tmpdir, userInfo } from 'node:os';
import { resolve, join } from 'node:path';
import { afterAll, beforeAll, expect, test } from 'vitest';

let root: string;
const servers: ChildProcess[] = [];
beforeAll(async () => {
  root = await mkdtemp(join(tmpdir(), 'xpilot-http-'));
  await mkdir(join(root, 'public/runtime'), { recursive: true });
  await writeFile(join(root, 'private.txt'), 'outside the web root');
  await writeFile(
    join(root, 'public/index.html'),
    '<!doctype html><title>XPilot</title>'
  );
  await writeFile(
    join(root, 'public/runtime/game.wasm'),
    Buffer.from([0, 97, 115, 109])
  );
  await writeFile(join(root, 'public/runtime/game.mjs'), 'export default 1;');
  await symlink('../private.txt', join(root, 'public/escape.txt'));
  await symlink('..', join(root, 'public/escape'));
  const large = await open(join(root, 'public/large.data'), 'w');
  await large.truncate(65 * 1024 * 1024);
  await large.close();
});
afterAll(async () => {
  for (const server of servers) {
    if (server.exitCode === null && server.signalCode === null) {
      server.kill('SIGTERM');
      await once(server, 'exit');
    }
  }
  await rm(root, { recursive: true, force: true });
});
const start = async (extra: string[]) => {
  const reservation = createServer();
  reservation.listen(0, '127.0.0.1');
  await once(reservation, 'listening');
  const address = reservation.address();
  if (!address || typeof address === 'string') throw new Error('No port');
  const port = address.port;
  await new Promise<void>((done) => reservation.close(() => done()));
  const server = spawn(
    process.env.XPILOT_SERVER_BINARY ??
      resolve('../build/xpilot-infinity/src/server/xpilot-infinity-server'),
    [
      '-map',
      resolve('tests/arena.xp2'),
      '-listen',
      `websocket:${port}`,
      '-webRoot',
      join(root, 'public'),
      '-noQuit',
      '+reportMeta',
      '-minRobots',
      '0',
      '-maxRobots',
      '0',
      ...extra,
    ]
  );
  servers.push(server);
  await new Promise<void>((done, reject) => {
    let log = '';
    const collect = (chunk: Buffer) => {
      log += chunk;
      if (log.includes('Server runs at')) done();
    };
    server.stdout.on('data', collect);
    server.stderr.on('data', collect);
    server.once('error', reject);
    server.once('exit', (code) =>
      reject(new Error(`Server exited ${code}: ${log}`))
    );
  });
  return port;
};
const request = async (
  port: number,
  message: string,
  record: Buffer | undefined
): Promise<Buffer> =>
  await new Promise((done, reject) => {
    const socket = connect(port, '127.0.0.1');
    const chunks: Buffer[] = [];
    socket.setTimeout(10000, () =>
      socket.destroy(new Error('HTTP response deadline'))
    );
    socket.on('error', reject);
    socket.on('connect', () => {
      socket.write(message);
      if (record) {
        // Client frames must be masked; this deterministic mask is only a fixture.
        const mask = Buffer.from([17, 23, 41, 53]);
        const payload = Buffer.from(record);
        for (let i = 0; i < payload.length; i++)
          payload[i] = payload[i]! ^ mask[i % 4]!;
        socket.write(
          Buffer.concat([
            Buffer.from([0x82, 0x80 | payload.length]),
            mask,
            payload,
          ])
        );
      }
    });
    socket.on('data', (chunk: Buffer) => {
      chunks.push(chunk);
      const data = Buffer.concat(chunks),
        split = data.indexOf('\r\n\r\n');
      if (split < 0) return;
      const headers = data.subarray(0, split).toString();
      if (headers.startsWith('HTTP/1.1 101')) {
        const body = data.subarray(split + 4);
        if (
          !record ||
          (body.length >= 2 && body.length >= 2 + (body[1]! & 127))
        ) {
          done(data);
          socket.destroy();
        }
      }
    });
    socket.on('end', () => done(Buffer.concat(chunks)));
  });
const upgrade = (port: number, headers: string) =>
  `GET /xpilot HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Protocol: xpilot-infinity-v1\r\n${headers}\r\n`;

test('serves bounded GET and HEAD responses without exposing paths outside webRoot', async () => {
  const port = await start([]);
  const get = (path: string, method: string) =>
    request(
      port,
      `${method} ${path} HTTP/1.1\r\nHost: localhost\r\n\r\n`,
      undefined
    );
  const index = (await get('/', 'GET')).toString();
  expect(index).toContain('HTTP/1.1 200 OK');
  expect(index).toContain('Content-Type: text/html');
  expect(index).toContain('<title>XPilot</title>');
  const head = (await get('/runtime/game.wasm', 'HEAD')).toString();
  expect(head).toContain('Content-Type: application/wasm');
  expect(head).toContain('Content-Length: 4');
  expect(head.endsWith('\r\n\r\n')).toBe(true);
  expect(
    (await get('/runtime/game.mjs?version=1', 'GET')).toString()
  ).toContain('text/javascript');
  const wasm = await get('/runtime/game.wasm', 'GET');
  expect(wasm.subarray(wasm.indexOf('\r\n\r\n') + 4)).toEqual(
    Buffer.from([0, 97, 115, 109])
  );
  for (const path of [
    '/../private.txt',
    '/%2e%2e/private.txt',
    '/%252e%252e/private.txt',
    '/escape.txt',
    '/escape/private.txt',
    '/runtime%2fgame.wasm',
    '/runtime\\game.wasm',
    '/%00',
    '/missing',
    '/runtime/',
  ]) {
    const response = (await get(path, 'GET')).toString();
    expect(response, path).toMatch(/^HTTP\/1.1 (400|403|404)/);
    expect(response).not.toContain('outside the web root');
  }
  expect((await get('/large.data', 'GET')).toString()).toContain('413');
  expect((await get('/', 'POST')).toString()).toContain('405');
  expect(
    (
      await request(
        port,
        'GET / HTTP/1.1\r\nHost: one\r\nHost: two\r\n\r\n',
        undefined
      )
    ).toString()
  ).toContain('400');
  expect(
    (
      await request(
        port,
        `GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: ${'x'.repeat(9000)}\r\n\r\n`,
        undefined
      )
    ).toString()
  ).toContain('431');
  const concurrent = await Promise.all(
    Array.from({ length: 8 }, () => get('/runtime/game.wasm', 'GET'))
  );
  expect(
    concurrent.every((value) => value.toString().startsWith('HTTP/1.1 200'))
  ).toBe(true);
});

test('checks browser Origin and never grants local administration to a WebSocket peer', async () => {
  const port = await start([]);
  expect(
    (
      await request(
        port,
        upgrade(port, `Origin: http://127.0.0.1:${port}\r\n`),
        undefined
      )
    ).toString()
  ).toContain('101 Switching Protocols');
  for (const origin of [
    'null',
    'https://other.example',
    `http://127.0.0.1:${port}\r\nOrigin: http://127.0.0.1:${port}`,
  ]) {
    expect(
      (
        await request(port, upgrade(port, `Origin: ${origin}\r\n`), undefined)
      ).toString()
    ).toMatch(/^HTTP\/1.1 (400|403)/);
  }
  // Native WS clients can omit Origin but still have no administration rights.
  const command = Buffer.concat([
    Buffer.from([0xf4, 0xed, 2, 1, 2, 0x4f, 0x1b, 0x45, 7]),
    Buffer.from(userInfo().username + '\0'),
    Buffer.from([0x67]),
    Buffer.from('gravity:0\0'),
  ]);
  const result = await request(port, upgrade(port, ''), command);
  const body = result.subarray(result.indexOf('\r\n\r\n') + 4);
  expect(body.subarray(2, 8)).toEqual(Buffer.from([0xf4, 0xed, 2, 3, 0x67, 1]));
});

test('requires a valid real address only from an explicitly trusted proxy', async () => {
  const port = await start([
    '-webOrigin',
    'https://game.example',
    '-webTrustedProxy',
    '127.0.0.1',
  ]);
  for (const header of [
    '',
    'X-Real-IP: invalid\r\n',
    'X-Real-IP: 192.0.2.1, 192.0.2.2\r\n',
    'X-Real-IP: 192.0.2.1\r\nX-Real-IP: 192.0.2.2\r\n',
  ])
    expect(
      (
        await request(
          port,
          upgrade(port, `Origin: https://game.example\r\n${header}`),
          undefined
        )
      ).toString()
    ).toMatch(/^HTTP\/1.1 (400|403)/);
  expect(
    (
      await request(
        port,
        upgrade(
          port,
          'Origin: https://game.example\r\nX-Real-IP: 192.0.2.1\r\n'
        ),
        undefined
      )
    ).toString()
  ).toContain('101 Switching Protocols');
  const direct = await start([
    '-webOrigin',
    'https://game.example',
    '-webTrustedProxy',
    '192.0.2.9',
  ]);
  expect(
    (
      await request(
        direct,
        upgrade(
          direct,
          'Origin: https://game.example\r\nX-Real-IP: forged\r\n'
        ),
        undefined
      )
    ).toString()
  ).toContain('101 Switching Protocols');
});

test('applies game admission limits to validated real addresses, ignoring untrusted spoofing', async () => {
  const peers: ReturnType<typeof connect>[] = [];
  const admit = async (
    port: number,
    real: string,
    name: string
  ): Promise<number> =>
    await new Promise((done, reject) => {
      const socket = connect(port, '127.0.0.1');
      peers.push(socket);
      socket.setTimeout(10000, () =>
        socket.destroy(new Error('Admission deadline'))
      );
      socket.on('error', reject);
      socket.on('connect', () => {
        const record = Buffer.concat([
          Buffer.from([0xf4, 0xed, 2, 1, 1, 0x4f, 0x1b, 0x45, 7]),
          Buffer.from(`browser\0${name}\0WebGL\0localhost\0`),
          Buffer.from([0xff, 0xff]),
        ]);
        const mask = Buffer.from([9, 7, 5, 3]);
        const masked = Buffer.from(record);
        for (let i = 0; i < masked.length; i++)
          masked[i] = masked[i]! ^ mask[i % 4]!;
        socket.write(
          Buffer.concat([
            Buffer.from(
              upgrade(
                port,
                `Origin: https://game.example\r\nX-Real-IP: ${real}\r\n`
              )
            ),
            Buffer.from([0x82, 0x80 | record.length]),
            mask,
            masked,
          ])
        );
      });
      let data = Buffer.alloc(0);
      socket.on('data', (chunk: Buffer) => {
        data = Buffer.concat([data, chunk]);
        const split = data.indexOf('\r\n\r\n');
        if (split < 0 || data.length < split + 11) return;
        const body = data.subarray(split + 4);
        if (body[0] !== 0x82 || body[2] !== 0xf4 || body[5] !== 2) {
          reject(new Error('Unexpected admission reply'));
          return;
        }
        done(body[6]!);
      });
    });
  try {
    const proxy = await start([
      '-webOrigin',
      'https://game.example',
      '-webTrustedProxy',
      '127.0.0.1',
      '-maxClientsPerIP',
      '1',
    ]);
    expect(await admit(proxy, '192.0.2.1', 'RealOne')).toBe(0);
    expect(await admit(proxy, '::ffff:c000:201', 'RealDuplicate')).toBe(5);
    expect(await admit(proxy, '192.0.2.2', 'RealTwo')).toBe(0);
    const direct = await start([
      '-webOrigin',
      'https://game.example',
      '-webTrustedProxy',
      '192.0.2.9',
      '-maxClientsPerIP',
      '1',
    ]);
    expect(await admit(direct, '192.0.2.1', 'DirectOne')).toBe(0);
    expect(await admit(direct, '192.0.2.2', 'DirectSpoof')).toBe(5);
  } finally {
    for (const peer of peers) peer.destroy();
  }
});
