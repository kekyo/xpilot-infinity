// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { mkdir, writeFile, readFile } from 'node:fs/promises';
import { createServer as createTCP, connect, type Socket } from 'node:net';
import { createServer as createTLS } from 'node:https';
import { request as requestHTTP } from 'node:http';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { expect, test } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

test.each(['http', 'https'])(
  'plays files supplied by the native server over %s',
  async (scheme) => {
    const output = resolve(`test-results/browser-hosting-${scheme}`);
    await mkdir(output, { recursive: true });
    const reservation = createTCP();
    reservation.listen(0, '127.0.0.1');
    await once(reservation, 'listening');
    const reserved = reservation.address();
    if (!reserved || typeof reserved === 'string') throw new Error('No port');
    const port = reserved.port;
    await new Promise<void>((done) => reservation.close(() => done()));
    const sockets = new Set<Socket>();
    const games = new Set<Socket>();
    let tls: ReturnType<typeof createTLS> | undefined;
    let url = `http://127.0.0.1:${port}`;
    let upgrades = 0;
    const served = new Set<string>();
    if (scheme === 'https') {
      const certificate = spawn(
        'openssl',
        [
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
        ],
        { stdio: 'ignore' }
      );
      expect((await once(certificate, 'exit'))[0]).toBe(0);
      tls = createTLS(
        {
          key: await readFile(resolve(output, 'key.pem')),
          cert: await readFile(resolve(output, 'cert.pem')),
        },
        (req, res) => {
          served.add(req.url!);
          const upstream = requestHTTP(
            {
              hostname: '127.0.0.1',
              port,
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
        const outgoing = connect(port, '127.0.0.1');
        games.add(outgoing);
        outgoing.on('close', () => {
          games.delete(outgoing);
          incoming.destroy();
        });
        incoming.on('close', () => outgoing.destroy());
        incoming.on('error', () => outgoing.destroy());
        outgoing.on('error', () => incoming.destroy());
        outgoing.on('connect', () => {
          // The test TLS frontend, like the documented proxy, overwrites identity.
          const headers = { ...req.headers, 'x-real-ip': '192.0.2.10' };
          outgoing.write(
            `GET ${req.url} HTTP/1.1\r\n${Object.entries(headers)
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
        throw new Error('No TLS port');
      url = `https://127.0.0.1:${address.port}`;
    }
    const binary =
      process.env.XPILOT_SERVER_BINARY ??
      resolve('../build/xpilot-infinity/src/server/xpilot-infinity-server');
    const server = spawn(binary, [
      '-map',
      resolve('tests/arena.xp2'),
      '-listen',
      `websocket:${port}`,
      '-webRoot',
      resolve('dist'),
      '-noQuit',
      '+reportMeta',
      '-minRobots',
      '0',
      '-maxRobots',
      '0',
      ...(scheme === 'https'
        ? ['-webOrigin', url, '-webTrustedProxy', '127.0.0.1']
        : []),
    ]);
    let log = '';
    let notifyResumed: () => void;
    const resumed = new Promise<void>((done) => {
      notifyResumed = done;
    });
    const ready = new Promise<void>((done, reject) => {
      const collect = (chunk: Buffer) => {
        log += chunk;
        if (log.includes('gameplay session resumed')) notifyResumed();
        if (log.includes('Server runs at')) done();
      };
      server.stdout.on('data', collect);
      server.stderr.on('data', collect);
      server.on('error', reject);
      server.on('exit', (code) =>
        reject(new Error(`Server exited ${code}: ${log}`))
      );
    });
    const client = new Client({
      name: 'xpilot-hosting-tests',
      version: '1.0.0',
    });
    let connected = false;
    const call = async (name: string, args: Record<string, unknown>) => {
      await writeFile(
        resolve(output, 'last-action.json'),
        JSON.stringify({ name, args })
      );
      const result = (await client.callTool({
        name,
        arguments: args,
      })) as CallToolResult;
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
      await ready;
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
          timeouts: { action: 15000, navigation: 30000 },
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
        '() => { document.querySelector("#nickname").value="HostedPilot"; document.querySelector("#mute").checked=true; document.querySelector("#mute").dispatchEvent(new Event("change")); document.querySelector("#save").click(); return true; }'
      );
      const configText = await evaluate(
        'async () => { const db=await new Promise((done,reject)=>{const r=indexedDB.open("xpilot-infinity");r.onsuccess=()=>done(r.result);r.onerror=()=>reject(r.error);}); try {for(;;){const v=await new Promise(done=>{const r=db.transaction("settings").objectStore("settings").get("player");r.onsuccess=()=>done(r.result);}); if(v?.config) return v.config; await new Promise(requestAnimationFrame);}} finally {db.close();} }'
      );
      expect(configText.length).toBeGreaterThan(100);
      await call('browser_navigate', { url });
      await call('browser_wait_for', { text: 'Ready to join' });
      expect(
        await evaluate(
          '() => ({name:document.querySelector("#nickname").value,muted:document.querySelector("#mute").checked})'
        )
      ).toEqual({ name: 'HostedPilot', muted: true });
      await evaluate(
        '() => {document.querySelector("#connect").focus();return true;}'
      );
      await call('browser_press_key', { key: 'Enter' });
      await call('browser_wait_for', { text: 'Playing' });
      await evaluate(
        'async () => {while(Number(document.querySelector("#game").dataset.frame)<30) await new Promise(requestAnimationFrame);return true;}'
      );
      if (scheme === 'https') {
        for (const socket of games) socket.destroy();
        await resumed;
        await evaluate(
          'async () => {while(document.querySelector("#connection").dataset.state!=="playing") await new Promise(requestAnimationFrame);return true;}'
        );
        expect(upgrades).toBeGreaterThan(1);
        expect(log).toContain('gameplay session resumed');
        expect(log).toContain('192.0.2.10');
        expect([...served].some((path) => path.endsWith('.wasm'))).toBe(true);
        expect([...served].some((path) => path.endsWith('.data'))).toBe(true);
      }
      await call('browser_take_screenshot', {
        filename: resolve(output, 'hosted.png'),
        type: 'png',
      });
      await evaluate(
        '() => {document.querySelector("#disconnect").click();return true;}'
      );
      await call('browser_wait_for', { text: 'Disconnected' });
    } catch (error) {
      if (connected) {
        await writeFile(
          resolve(output, 'console.log'),
          await call('browser_console_messages', { level: 'debug' })
        );
        await call('browser_take_screenshot', {
          filename: resolve(output, 'failure.png'),
          type: 'png',
        });
      }
      throw error;
    } finally {
      if (connected) await client.close();
      for (const socket of games) socket.destroy();
      for (const socket of sockets) socket.destroy();
      if (tls) await new Promise<void>((done) => tls!.close(() => done()));
      if (server.exitCode === null && server.signalCode === null) {
        server.kill('SIGTERM');
        await once(server, 'exit');
      }
      await writeFile(resolve(output, 'server.log'), log);
    }
  },
  120000
);
