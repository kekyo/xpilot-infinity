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
import { preview } from 'vite';
import { test, expect } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

test('admits the browser to the native game through its content origin', async () => {
  const output = resolve('test-results/browser-session');
  await mkdir(output, { recursive: true });
  const reservation = createServer();
  reservation.listen(0, '127.0.0.1');
  await once(reservation, 'listening');
  const address = reservation.address();
  if (!address || typeof address === 'string')
    throw new Error('No reserved port');
  const port = address.port;
  await new Promise<void>((done) => reservation.close(() => done()));
  const binary =
    process.env.XPILOT_SERVER_BINARY ??
    resolve('../build/xpilot-infinity/src/server/xpilot-infinity-server');
  const server = spawn(binary, [
    '-map',
    resolve('../lib/maps/ndh.xp2'),
    '-listen',
    `websocket:${port}`,
    '-noQuit',
    '+reportMeta',
    '-maxClientsPerIP',
    '8',
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
  let host: Awaited<ReturnType<typeof preview>> | undefined;
  const client = new Client({ name: 'xpilot-session-tests', version: '1.0.0' });
  let connected = false;
  const call = async (name: string, args: Record<string, unknown>) => {
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
    const result = await call('browser_evaluate', { function: body });
    return JSON.parse(result.split('### Result\n')[1]!.split('\n###')[0]!);
  };
  try {
    await ready;
    host = await preview({
      configFile: false,
      preview: {
        host: '127.0.0.1',
        port: 0,
        proxy: { '/xpilot': { target: `ws://127.0.0.1:${port}`, ws: true } },
      },
    });
    const addr = host.httpServer.address();
    if (!addr || typeof addr === 'string') throw new Error('No preview port');
    const configPath = resolve(output, 'mcp.json');
    await writeFile(
      configPath,
      JSON.stringify({
        browser: {
          browserName: 'chromium',
          isolated: true,
          launchOptions: {
            channel: 'chrome',
            headless: true,
            args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
            ...(process.env.XPILOT_TEST_BROWSER
              ? { executablePath: process.env.XPILOT_TEST_BROWSER }
              : {}),
          },
        },
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
          configPath,
        ],
      })
    );
    connected = true;
    await call('browser_navigate', { url: `http://127.0.0.1:${addr.port}` });
    await call('browser_wait_for', { text: 'Rendering ready' });
    await evaluate(
      '() => { document.querySelector("#join").requestSubmit(); return true; }'
    );
    await call('browser_wait_for', {
      text: 'Session admitted; initial data received',
    });
    expect(
      await evaluate(
        '() => document.querySelector("#connection").dataset.state'
      )
    ).toBe('admitted');
    await evaluate(
      '() => { document.querySelector("#disconnect").click(); return true; }'
    );
    expect(
      await evaluate(
        '() => document.querySelector("#connection").dataset.state'
      )
    ).toBe('disconnected');
    server.kill('SIGTERM');
    await once(server, 'exit');
    await evaluate(
      '() => { document.querySelector("#join").requestSubmit(); return true; }'
    );
    await evaluate(
      'async () => { while(document.querySelector("#connection").dataset.state === "connecting") await new Promise(requestAnimationFrame); return true; }'
    );
    expect(
      await evaluate(
        '() => document.querySelector("#connection").dataset.state'
      )
    ).toBe('error');
    expect(
      await evaluate('() => document.querySelector("#status").dataset.state')
    ).toBe('ready');
  } finally {
    if (connected) await client.close();
    if (host && 'closeAllConnections' in host.httpServer)
      host.httpServer.closeAllConnections();
    if (host)
      await new Promise<void>((done) => host!.httpServer.close(() => done()));
    if (server.exitCode === null && server.signalCode === null) {
      server.kill('SIGTERM');
      await once(server, 'exit');
    }
    await writeFile(resolve(output, 'server.log'), log);
  }
}, 90000);
