// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:net';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { test, expect } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

test.each(['firefox', 'chromium'] as const)(
  '%s keeps fullscreen controls responsive without redundant paints',
  async (browserName) => {
    const output = resolve(`test-results/browser-fullscreen-${browserName}`);
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
    const client = new Client({
      name: 'xpilot-fullscreen-tests',
      version: '1.0.0',
    });
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
    try {
      await ready;
      const config = resolve(output, 'mcp.json');
      await writeFile(
        config,
        JSON.stringify({
          browser: {
            browserName,
            isolated: true,
            // Firefox must use a real fullscreen window, including its transition.
            contextOptions: {
              viewport:
                browserName === 'firefox'
                  ? { width: 3840, height: 1600 }
                  : { width: 1280, height: 800 },
            },
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
            '-s',
            '-screen 0 3840x1600x24',
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
      // Headed Firefox can play through the host even on a virtual display.
      await call('browser_click', { target: '#mute' });
      await call('browser_click', { target: '#connect' });
      await call('browser_wait_for', { text: 'Playing' });
      await call('browser_start_video', {
        filename: resolve(output, 'controls.webm'),
      });
      recording = true;
      await call('browser_click', { target: '#fullscreen' });
      expect(
        await evaluate(`async () => {
        while (!document.fullscreenElement) await new Promise(requestAnimationFrame);
        await new Promise(requestAnimationFrame);
        return { fullscreen: document.fullscreenElement.id, focus: document.activeElement.id };
      }`)
      ).toEqual({ fullscreen: 'game', focus: 'game' });

      // Count actual canvas clears between presentation opportunities. Extra
      // network messages must not cause unbounded, invisible redraws here.
      const paints: number[] = await evaluate(`async () => {
        const gl = document.querySelector('#game').getContext('webgl2');
        const clear = gl.clear;
        let paints = 0;
        gl.clear = (...args) => { paints++; return clear.apply(gl, args); };
        const samples = [];
        try {
          await new Promise(requestAnimationFrame);
          for (let frame = 0; frame < 20; frame++) {
            paints = 0;
            await new Promise(requestAnimationFrame);
            samples.push(paints);
          }
          return samples;
        } finally { gl.clear = clear; }
      }`);
      await writeFile(resolve(output, 'paints.json'), JSON.stringify(paints));
      expect(paints.some((count) => count > 0)).toBe(true);
      expect(Math.max(...paints)).toBeLessThanOrEqual(1);

      for (const fullscreen of [true, false]) {
        if (!fullscreen)
          await evaluate(
            'async () => { await document.exitFullscreen(); return true; }'
          );
        const result = await evaluate(`async () => {
          const canvas = document.querySelector('#game'), target = document.activeElement;
          const heading = canvas.dataset.heading;
          target.dispatchEvent(new KeyboardEvent('keydown', { key:'a', code:'KeyA', keyCode:65, bubbles:true }));
          while (canvas.dataset.heading === heading) await new Promise(requestAnimationFrame);
          const held = Number(canvas.dataset.held);
          target.dispatchEvent(new KeyboardEvent('keyup', { key:'a', code:'KeyA', keyCode:65, bubbles:true }));
          while (Number(canvas.dataset.held)) await new Promise(requestAnimationFrame);
          const turned = canvas.dataset.heading !== heading;
          const x = Number(canvas.dataset.shipX), y = Number(canvas.dataset.shipY);
          target.dispatchEvent(new KeyboardEvent('keydown', { key:'Shift', code:'ShiftLeft', keyCode:16, bubbles:true }));
          while (Math.hypot(Number(canvas.dataset.shipX)-x, Number(canvas.dataset.shipY)-y) < 30) await new Promise(requestAnimationFrame);
          target.dispatchEvent(new KeyboardEvent('keyup', { key:'Shift', code:'ShiftLeft', keyCode:16, bubbles:true }));
          while (Number(canvas.dataset.held)) await new Promise(requestAnimationFrame);
          return { focus: target.id, held, turned, released: Number(canvas.dataset.held) };
        }`);
        expect(result).toEqual({
          focus: 'game',
          held: 1,
          turned: true,
          released: 0,
        });
      }
      expect(
        await evaluate(
          '() => Number(document.querySelector("#audio").dataset.peak)'
        )
      ).toBe(0);
      await call('browser_click', { target: '#disconnect' });
      await call('browser_wait_for', { text: 'Disconnected' });
    } finally {
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
