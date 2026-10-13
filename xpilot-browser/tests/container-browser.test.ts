// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { expect, test } from 'vitest';
import { execFile, spawn } from 'node:child_process';
import { promisify } from 'node:util';
import { once } from 'node:events';
import { randomUUID } from 'node:crypto';
import { mkdir, readFile, stat, writeFile } from 'node:fs/promises';
import { connect, type Socket } from 'node:net';
import { createServer as createTLS } from 'node:https';
import { request } from 'node:http';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

const platforms = (
  process.env.XPILOT_CONTAINER_PLATFORMS ?? 'linux/amd64,linux/arm64'
).split(',');
test
  .runIf(process.env.XPILOT_BROWSER_CONTAINER_IMAGE !== undefined)
  .each(
    platforms.flatMap((platform) =>
      ['http', 'https'].map((scheme) => ({ platform, scheme }))
    )
  )(
  'plays the bundled container browser on $platform over $scheme',
  async ({ platform, scheme }) => {
    const output = resolve(
      `test-results/container-${platform.split('/')[1]}-${scheme}`
    );
    await mkdir(output, { recursive: true });
    const engine = process.env.CONTAINER_ENGINE ?? 'podman';
    const name = `xpilot-browser-container-${randomUUID()}`;
    const exec = promisify(execFile);
    const run = async (args: string[]) =>
      (await exec(engine, args, { timeout: 180000 })).stdout.trim();
    let backendPort = 0;
    let url = '';
    let tls: ReturnType<typeof createTLS> | undefined;
    const sockets = new Set<Socket>();
    let upgrades = 0;
    if (scheme === 'https') {
      await exec('openssl', [
        'req',
        '-x509',
        '-newkey',
        'rsa:2048',
        '-nodes',
        '-keyout',
        resolve(output, 'key.pem'),
        '-out',
        resolve(output, 'cert.pem'),
        '-days',
        '1',
        '-subj',
        '/CN=localhost',
        '-addext',
        'subjectAltName=IP:127.0.0.1',
      ]);
      tls = createTLS(
        {
          key: await readFile(resolve(output, 'key.pem')),
          cert: await readFile(resolve(output, 'cert.pem')),
        },
        (req, res) => {
          const upstream = request(
            {
              hostname: '127.0.0.1',
              port: backendPort,
              path: req.url,
              method: req.method,
              headers: { ...req.headers, connection: 'close' },
            },
            (reply) => {
              res.writeHead(reply.statusCode!, reply.headers);
              reply.pipe(res);
            }
          );
          upstream.on('error', () => {
            res.writeHead(502);
            res.end();
          });
          req.pipe(upstream);
        }
      );
      tls.on('connection', (socket) => {
        sockets.add(socket);
        socket.on('close', () => sockets.delete(socket));
      });
      tls.on('upgrade', (req, incoming, head) => {
        upgrades++;
        expect(req.url).toBe('/xpilot');
        expect(req.headers.origin).toBe(url);
        const outgoing = connect(backendPort, '127.0.0.1');
        sockets.add(outgoing);
        outgoing.on('close', () => {
          sockets.delete(outgoing);
          incoming.destroy();
        });
        incoming.on('close', () => outgoing.destroy());
        incoming.on('error', () => outgoing.destroy());
        outgoing.on('error', () => incoming.destroy());
        outgoing.on('connect', () => {
          outgoing.write(
            `GET ${req.url} HTTP/1.1\r\n${Object.entries(req.headers)
              .map(([key, value]) => `${key}: ${value}`)
              .join('\r\n')}\r\n\r\n`
          );
          if (head.length) outgoing.write(head);
          incoming.pipe(outgoing);
          outgoing.pipe(incoming);
        });
      });
      tls.listen(0, '127.0.0.1');
      await once(tls, 'listening');
      const address = tls.address();
      if (!address || typeof address === 'string')
        throw new Error('Missing TLS address');
      url = `https://127.0.0.1:${address.port}`;
    }
    const client = new Client({
      name: 'xpilot-container-tests',
      version: '1.0.0',
    });
    let connected = false;
    let recording = false;
    const call = async (name: string, args: Record<string, unknown>) => {
      const result = (await client.callTool(
        { name, arguments: args },
        undefined,
        { timeout: 180000 }
      )) as CallToolResult;
      expect(result.isError, JSON.stringify(result.content)).not.toBe(true);
      return result.content
        .filter((item) => item.type === 'text')
        .map((item) => item.text)
        .join('\n');
    };
    const evaluate = async (body: string) =>
      JSON.parse(
        (await call('browser_evaluate', { function: body }))
          .split('### Result\n')[1]!
          .split('\n###')[0]!
      );
    try {
      await run([
        'run',
        '-d',
        '--name',
        name,
        '--platform',
        platform,
        '--read-only',
        '--cap-drop=all',
        '--security-opt=no-new-privileges',
        '-p',
        '127.0.0.1::15347/tcp',
        '-v',
        `${resolve('tests/arena.xp2')}:/maps/arena.xp2:ro`,
        `${process.env.XPILOT_BROWSER_CONTAINER_IMAGE!}-${platform.split('/')[1]}`,
        '-map',
        '/maps/arena.xp2',
        '-maxClientsPerIP',
        '8',
        ...(scheme === 'https' ? ['-webOrigin', url] : []),
      ]);
      const logs = spawn(engine, ['logs', '--follow', name], {
        timeout: 180000,
      });
      let serverLog = '';
      try {
        await new Promise<void>((done, reject) => {
          const collect = (chunk: Buffer) => {
            serverLog += chunk.toString();
            if (serverLog.includes('Server runs at')) done();
          };
          logs.stdout.on('data', collect);
          logs.stderr.on('data', collect);
          logs.once('error', reject);
          logs.once('exit', (code) =>
            reject(new Error(`Server exited ${code}: ${serverLog}`))
          );
        });
      } finally {
        const closed = once(logs, 'close');
        logs.kill('SIGTERM');
        await closed;
      }
      const mapping = await run(['port', name]);
      expect(mapping).toMatch(/^15347\/tcp -> 127\.0\.0\.1:\d+$/);
      backendPort = Number(mapping.slice(mapping.lastIndexOf(':') + 1));
      expect(await run(['exec', name, 'node', '-p', 'process.arch'])).toBe(
        platform.endsWith('amd64') ? 'x64' : 'arm64'
      );
      if (scheme === 'http') url = `http://127.0.0.1:${backendPort}`;
      const config = resolve(output, 'mcp.json');
      await writeFile(
        config,
        JSON.stringify({
          browser: {
            browserName: 'chromium',
            isolated: true,
            contextOptions: { ignoreHTTPSErrors: true },
            launchOptions: {
              channel: 'chrome',
              headless: true,
              args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
              ...(process.env.XPILOT_TEST_BROWSER
                ? { executablePath: process.env.XPILOT_TEST_BROWSER }
                : {}),
            },
          },
          capabilities: ['core', 'devtools'],
          outputDir: output,
          timeouts: { action: 120000, navigation: 120000 },
        })
      );
      const require = createRequire(import.meta.url);
      await client.connect(
        new StdioClientTransport({
          command: process.execPath,
          args: [
            resolve(
              dirname(require.resolve('@playwright/mcp/package.json')),
              'cli.js'
            ),
            '--config',
            config,
          ],
        })
      );
      connected = true;
      await call('browser_navigate', { url });
      await call('browser_wait_for', { text: 'Ready to join' });
      await evaluate(
        '() => {document.querySelector("#nickname").value="ContainerPilot";document.querySelector("#join").requestSubmit();return true;}'
      );
      await call('browser_wait_for', { text: 'Playing' });
      await call('browser_start_video', {
        filename: resolve(output, 'play.webm'),
      });
      recording = true;
      const start = await evaluate(
        'async () => {const c=document.querySelector("#game");while(Number(c.dataset.frame)<30)await new Promise(requestAnimationFrame);return Number(c.dataset.frame);}'
      );
      expect(
        await evaluate(
          'async () => {const c=document.querySelector("#game");c.focus();const x=Number(c.dataset.shipX),y=Number(c.dataset.shipY);c.dispatchEvent(new KeyboardEvent("keydown",{key:"Shift",code:"ShiftLeft",keyCode:16,bubbles:true}));while(Math.hypot(Number(c.dataset.shipX)-x,Number(c.dataset.shipY)-y)<30)await new Promise(requestAnimationFrame);c.dispatchEvent(new KeyboardEvent("keyup",{key:"Shift",code:"ShiftLeft",keyCode:16,bubbles:true}));return true;}'
        )
      ).toBe(true);
      expect(
        await evaluate(
          'async () => {const c=document.querySelector("#game");c.dispatchEvent(new KeyboardEvent("keydown",{key:"Enter",code:"Enter",keyCode:13,bubbles:true}));while(Number(c.dataset.shots)===0)await new Promise(requestAnimationFrame);c.dispatchEvent(new KeyboardEvent("keyup",{key:"Enter",code:"Enter",keyCode:13,bubbles:true}));return Number(c.dataset.shots);}'
        )
      ).toBeGreaterThan(0);
      await evaluate(
        '() => {document.querySelector("#chat").value="Container chat";document.querySelector("#chat-form").requestSubmit();return true;}'
      );
      await call('browser_wait_for', { text: 'Container chat' });
      expect(
        await evaluate(
          '() => Number(document.querySelector("#game").dataset.frame)'
        )
      ).toBeGreaterThan(start);
      await call('browser_take_screenshot', {
        filename: resolve(output, 'playing.png'),
        type: 'png',
      });
      await call('browser_stop_video', {});
      recording = false;
      expect((await stat(resolve(output, 'play.webm'))).size).toBeGreaterThan(
        5000
      );
      await evaluate(
        '() => {document.querySelector("#disconnect").click();return true;}'
      );
      await call('browser_wait_for', { text: 'Disconnected' });
      await evaluate(
        '() => {document.querySelector("#join").requestSubmit();return true;}'
      );
      await call('browser_wait_for', { text: 'Playing' });
      if (scheme === 'https') expect(upgrades).toBeGreaterThanOrEqual(2);
    } finally {
      if (recording) await call('browser_stop_video', {});
      if (connected) await client.close();
      for (const socket of sockets) socket.destroy();
      if (tls) await new Promise<void>((done) => tls!.close(() => done()));
      await run(['stop', '--ignore', '--time', '60', name]);
      await writeFile(resolve(output, 'server.log'), await run(['logs', name]));
      await run(['rm', '--force', '--ignore', name]);
    }
  },
  600000
);
