// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { mkdir, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { createServer } from 'node:net';
import { dirname, resolve } from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { expect, test } from 'vitest';

test.each(['firefox', 'chromium'] as const)(
  '%s returns keyboard control to the game while preserving chat input',
  async (browserName) => {
    const output = resolve(`test-results/browser-focus-${browserName}`);
    await mkdir(output, { recursive: true });
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
        '-defaults',
        resolve('../lib/defaults.txt'),
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
      ]
    );
    let serverLog = '';
    const ready = new Promise<void>((done, reject) => {
      const collect = (chunk: Buffer) => {
        serverLog += chunk.toString();
        if (serverLog.includes('Server runs at')) done();
      };
      server.stdout.on('data', collect);
      server.stderr.on('data', collect);
      server.once('error', reject);
      server.once('exit', (code) =>
        reject(new Error(`Server exited ${code}: ${serverLog}`))
      );
    });
    const client = new Client({ name: 'xpilot-focus-tests', version: '1.0.0' });
    let connected = false;
    let recording = false;
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
    const evaluate = async (body: string) => {
      const text = await call('browser_evaluate', { function: body });
      return JSON.parse(text.split('### Result\n')[1]!.split('\n###')[0]!);
    };
    const checkKeyboard = async () => {
      expect(
        await evaluate(`() => ({
          focus: document.activeElement.id,
          pointer: document.pointerLockElement?.id ?? null
        })`)
      ).toEqual({ focus: 'game', pointer: null });
      expect(
        await evaluate(`async () => {
          const canvas = document.querySelector('#game');
          const target = document.activeElement, heading = canvas.dataset.heading;
          target.dispatchEvent(new KeyboardEvent('keydown', { key:'a', code:'KeyA', bubbles:true }));
          try {
            while (canvas.dataset.heading === heading) await new Promise(requestAnimationFrame);
            return Number(canvas.dataset.held);
          } finally {
            target.dispatchEvent(new KeyboardEvent('keyup', { key:'a', code:'KeyA', bubbles:true }));
          }
        }`)
      ).toBe(1);
      expect(
        await evaluate(`async () => {
          const canvas = document.querySelector('#game');
          while (Number(canvas.dataset.held)) await new Promise(requestAnimationFrame);
          return Number(canvas.dataset.held);
        }`)
      ).toBe(0);
    };
    try {
      await ready;
      const config = resolve(output, 'mcp.json');
      await writeFile(
        config,
        JSON.stringify({
          browser: {
            browserName,
            isolated: true,
            contextOptions: { viewport: { width: 1280, height: 900 } },
            launchOptions:
              browserName === 'firefox'
                ? { headless: false }
                : {
                    channel: 'chrome',
                    headless: true,
                    args: [
                      '--use-angle=swiftshader',
                      '--enable-unsafe-swiftshader',
                    ],
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
          command: 'xvfb-run',
          args: [
            '-a',
            process.execPath,
            resolve(
              dirname(require.resolve('@playwright/mcp/package.json')),
              'cli.js'
            ),
            '--config',
            config,
          ],
          env: {
            ...Object.fromEntries(
              Object.entries(process.env).filter(
                (entry): entry is [string, string] => entry[1] !== undefined
              )
            ),
            LIBGL_ALWAYS_SOFTWARE: '1',
          },
        })
      );
      connected = true;
      await call('browser_navigate', { url: `http://127.0.0.1:${port}/` });
      await call('browser_wait_for', { text: 'Ready to join' });
      await call('browser_click', { target: '#mute' });
      await call('browser_click', { target: '#connect' });
      await call('browser_wait_for', { text: 'Playing' });
      await call('browser_start_video', {
        filename: resolve(output, 'focus.webm'),
      });
      recording = true;
      await checkKeyboard();

      for (const target of ['#save', '#mute', '#mute']) {
        await call('browser_click', { target });
        await checkKeyboard();
      }

      await call('browser_type', { target: '#chat', text: 'as', slowly: true });
      expect(
        await evaluate(`async () => {
          const canvas = document.querySelector('#game');
          const frame = Number(canvas.dataset.frame);
          while (Number(canvas.dataset.frame) <= frame + 2) await new Promise(requestAnimationFrame);
          return { focus: document.activeElement.id, text: document.querySelector('#chat').value, held: Number(canvas.dataset.held) };
        }`)
      ).toEqual({ focus: 'chat', text: 'as', held: 0 });
      await call('browser_click', { target: '#game' });
      await checkKeyboard();
      await call('browser_click', { target: '#chat-form button' });
      await checkKeyboard();

      await call('browser_click', { target: '#mouse' });
      expect(
        await evaluate(`async () => {
          while (!document.pointerLockElement) await new Promise(requestAnimationFrame);
          const canvas = document.querySelector('#game'), heading = canvas.dataset.heading;
          canvas.dispatchEvent(new PointerEvent('pointermove', { pointerType:'mouse', pointerId:1, bubbles:true, movementX:80, movementY:0 }));
          while (canvas.dataset.heading === heading) await new Promise(requestAnimationFrame);
          return document.pointerLockElement.id;
        }`)
      ).toBe('game');
      await evaluate(`async () => {
        document.exitPointerLock();
        while (document.pointerLockElement) await new Promise(requestAnimationFrame);
        return true;
      }`);
      await checkKeyboard();

      // Stop the local server to make the pending admission deterministic.
      // Focus changes must survive or be repaired when gameplay actually starts.
      for (const target of ['h1', '#chat']) {
        await call('browser_click', { target: '#disconnect' });
        await call('browser_wait_for', { text: 'Disconnected' });
        server.kill('SIGSTOP');
        await call('browser_click', { target: '#connect' });
        await call('browser_wait_for', { text: 'Connecting…' });
        await call('browser_click', { target });
        if (target === '#chat')
          await call('browser_type', { target, text: 'draft', slowly: true });
        server.kill('SIGCONT');
        await call('browser_wait_for', { text: 'Playing' });
        if (target === '#chat') {
          expect(
            await evaluate(
              `() => ({ focus: document.activeElement.id, text: document.querySelector('#chat').value })`
            )
          ).toEqual({ focus: 'chat', text: 'draft' });
          await call('browser_click', { target: '#chat-form button' });
          await call('browser_wait_for', { text: 'draft' });
        }
        await checkKeyboard();
      }
      await call('browser_click', { target: '#disconnect' });
      await call('browser_wait_for', { text: 'Disconnected' });
    } finally {
      server.kill('SIGCONT');
      if (recording) await call('browser_stop_video', {});
      if (connected) await client.close();
      if (server.exitCode === null && server.signalCode === null) {
        server.kill('SIGTERM');
        await once(server, 'exit');
      }
      await writeFile(resolve(output, 'server.log'), serverLog);
    }
  },
  90000
);
