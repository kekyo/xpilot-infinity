// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { mkdir, writeFile, stat, readFile } from 'node:fs/promises';
import { createServer, connect as connectTCP, type Socket } from 'node:net';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { preview } from 'vite';
import { test, expect } from 'vitest';
import { PNG } from 'pngjs';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

test('plays the native game in WebAssembly', async () => {
  const output = resolve('test-results/browser-game');
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
    resolve('tests/arena.xp2'),
    '-listen',
    `websocket:${port}`,
    '-noQuit',
    '+reportMeta',
    '-minRobots',
    '0',
    '-maxRobots',
    '0',
    '-maxClientsPerIP',
    '8',
  ]);
  let log = '';
  let notifyResumed: () => void;
  const resumed = new Promise<void>((done) => {
    notifyResumed = done;
  });
  const ready = new Promise<void>((done, reject) => {
    const collect = (chunk: Buffer) => {
      log += chunk.toString();
      if (log.includes('gameplay session resumed')) notifyResumed();
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
  let recording = false;
  let native: ReturnType<typeof spawn> | undefined;
  let nativeLog = '';
  let notifyNative: () => void;
  const nativeJoined = new Promise<void>((done) => {
    notifyNative = done;
  });
  const streams = new Set<Socket>();
  const httpStreams = new Set<Socket>();
  const bridge = createServer((incoming) => {
    const outgoing = connectTCP(port, '127.0.0.1');
    streams.add(incoming);
    streams.add(outgoing);
    incoming.on('error', () => outgoing.destroy());
    outgoing.on('error', () => incoming.destroy());
    incoming.on('close', () => {
      streams.delete(incoming);
      outgoing.destroy();
    });
    outgoing.on('close', () => {
      streams.delete(outgoing);
      incoming.destroy();
    });
    incoming.pipe(outgoing);
    outgoing.pipe(incoming);
  });
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
    const result = await call('browser_evaluate', { function: body });
    return JSON.parse(result.split('### Result\n')[1]!.split('\n###')[0]!);
  };
  try {
    await ready;
    bridge.listen(0, '127.0.0.1');
    await once(bridge, 'listening');
    const bridgeAddress = bridge.address();
    if (!bridgeAddress || typeof bridgeAddress === 'string')
      throw new Error('No bridge');
    host = await preview({
      configFile: false,
      preview: {
        host: '127.0.0.1',
        port: 0,
        proxy: {
          '/xpilot': {
            target: `ws://127.0.0.1:${bridgeAddress.port}`,
            ws: true,
          },
        },
      },
    });
    // HTTP closeAllConnections deliberately excludes upgraded WebSockets.
    host.httpServer.on('connection', (socket) => {
      httpStreams.add(socket);
      socket.on('close', () => httpStreams.delete(socket));
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
          contextOptions: { deviceScaleFactor: 2 },
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
          configPath,
        ],
      })
    );
    connected = true;
    await call('browser_navigate', { url: `http://127.0.0.1:${addr.port}` });
    await call('browser_wait_for', { text: 'Ready to join' });
    expect(await evaluate('() => !!document.querySelector("#mute")')).toBe(
      true
    );
    await evaluate(
      '() => { const n=document.querySelector("#nickname"); n.value="StoredPilot"; n.dispatchEvent(new Event("change")); return true; }'
    );
    await call('browser_wait_for', { text: 'Settings saved' });
    await call('browser_navigate', { url: `http://127.0.0.1:${addr.port}` });
    await call('browser_wait_for', { text: 'Ready to join' });
    expect(
      await evaluate('() => document.querySelector("#nickname").value')
    ).toBe('StoredPilot');
    await evaluate(
      '() => { document.querySelector("#nickname").value="BrowserPilot"; document.querySelector("#connect").focus(); return true; }'
    );
    await call('browser_press_key', { key: 'Enter' });
    await call('browser_wait_for', {
      text: 'Playing',
    });
    expect(
      await evaluate(
        '() => document.querySelector("#connection").dataset.state'
      )
    ).toBe('playing');
    await call('browser_start_video', {
      filename: resolve(output, 'play.webm'),
    });
    recording = true;
    await evaluate(
      'async () => { const c=document.querySelector("#game"); while (Number(c.dataset.frame) < 60) await new Promise(requestAnimationFrame); return true; }'
    );
    await call('browser_take_screenshot', {
      filename: resolve(output, 'playing.png'),
      type: 'png',
      target: '#game',
    });
    const pixels = PNG.sync.read(
      await readFile(resolve(output, 'playing.png'))
    );
    const colors = new Set<string>();
    for (let i = 0; i < pixels.data.length; i += 4)
      colors.add(pixels.data.subarray(i, i + 3).join(','));
    expect(colors.size).toBeGreaterThan(30);
    const dimensions = await evaluate(
      '() => { const c=document.querySelector("#game"); return {width:c.width,css:c.getBoundingClientRect().width,logical:Number(c.dataset.logicalWidth)}; }'
    );
    expect(dimensions.width).toBe(Math.round(dimensions.css * 2));
    expect(dimensions.logical).toBe(Math.round(dimensions.css));
    const steering = await evaluate(
      'async () => { const c=document.querySelector("#game"); c.focus(); const h=c.dataset.heading; c.dispatchEvent(new KeyboardEvent("keydown",{key:"a",code:"KeyA",keyCode:65,bubbles:true})); while(c.dataset.heading===h) await new Promise(requestAnimationFrame); c.dispatchEvent(new KeyboardEvent("keyup",{key:"a",code:"KeyA",keyCode:65,bubbles:true})); return c.dataset.heading !== h; }'
    );
    expect(steering).toBe(true);
    const moved = await evaluate(
      'async () => { const c=document.querySelector("#game"); const x=Number(c.dataset.shipX),y=Number(c.dataset.shipY); c.dispatchEvent(new KeyboardEvent("keydown",{key:"Shift",code:"ShiftLeft",keyCode:16,bubbles:true})); while(Math.hypot(Number(c.dataset.shipX)-x,Number(c.dataset.shipY)-y)<30) await new Promise(requestAnimationFrame); c.dispatchEvent(new KeyboardEvent("keyup",{key:"Shift",code:"ShiftLeft",keyCode:16,bubbles:true})); return true; }'
    );
    expect(moved).toBe(true);
    expect(
      await evaluate(
        'async () => { const c=document.querySelector("#game"); c.dispatchEvent(new KeyboardEvent("keydown",{key:"Enter",code:"Enter",keyCode:13,bubbles:true})); while(Number(c.dataset.shots)===0) await new Promise(requestAnimationFrame); c.dispatchEvent(new KeyboardEvent("keyup",{key:"Enter",code:"Enter",keyCode:13,bubbles:true})); return Number(c.dataset.shots); }'
      )
    ).toBeGreaterThan(0);
    await evaluate(
      'async () => { while(Number(document.querySelector("#audio").dataset.peak)<=0) await new Promise(requestAnimationFrame); return true; }'
    );
    await evaluate(
      'async () => { const c=document.querySelector("#game"); c.dispatchEvent(new KeyboardEvent("keydown",{key:"a",code:"KeyA",keyCode:65,bubbles:true})); while(Number(c.dataset.held)===0) await new Promise(requestAnimationFrame); window.dispatchEvent(new Event("blur")); while(Number(c.dataset.held)!==0) await new Promise(requestAnimationFrame); c.dispatchEvent(new KeyboardEvent("keyup",{key:"a",code:"KeyA",keyCode:65,bubbles:true})); return true; }'
    );
    await evaluate(
      'async () => { const c=document.querySelector("#game"); const ext=c.getContext("webgl2").getExtension("WEBGL_lose_context"); const lost=new Promise(done=>c.addEventListener("webglcontextlost",done,{once:true})); ext.loseContext(); await lost; await new Promise(requestAnimationFrame); const restored=new Promise(done=>c.addEventListener("webglcontextrestored",done,{once:true})); ext.restoreContext(); await restored; while(document.querySelector("#connection").dataset.state!=="playing") await new Promise(requestAnimationFrame); return true; }'
    );
    await call('browser_take_screenshot', {
      filename: resolve(output, 'restored.png'),
      type: 'png',
      target: '#game',
    });
    const restored = PNG.sync.read(
      await readFile(resolve(output, 'restored.png'))
    );
    const restoredColors = new Set<string>();
    for (let i = 0; i < restored.data.length; i += 4)
      restoredColors.add(restored.data.subarray(i, i + 3).join(','));
    expect(restoredColors.size).toBeGreaterThan(30);
    await evaluate(
      '() => { document.querySelector("#mouse").focus(); return true; }'
    );
    await call('browser_press_key', { key: 'Enter' });
    expect(
      await evaluate(
        'async () => { while(!document.pointerLockElement) await new Promise(requestAnimationFrame); return document.pointerLockElement.id; }'
      )
    ).toBe('game');
    expect(
      await evaluate(
        'async () => { const c=document.querySelector("#game"), heading=c.dataset.heading; c.dispatchEvent(new PointerEvent("pointermove",{pointerType:"mouse",pointerId:1,bubbles:true,movementX:80,movementY:0})); while(c.dataset.heading===heading) await new Promise(requestAnimationFrame); return true; }'
      )
    ).toBe(true);
    await evaluate(
      '() => { document.exitPointerLock(); document.querySelector("#fullscreen").focus(); return true; }'
    );
    await call('browser_press_key', { key: 'Enter' });
    expect(
      await evaluate(
        'async () => { while(!document.fullscreenElement) await new Promise(requestAnimationFrame); return true; }'
      )
    ).toBe(true);
    await evaluate(
      'async () => { await document.exitFullscreen(); return true; }'
    );
    await call('browser_resize', { width: 1000, height: 780 });
    await evaluate(
      'async () => { const c=document.querySelector("#game"); while(Number(c.dataset.logicalWidth)!==document.querySelector("#viewport").clientWidth) await new Promise(requestAnimationFrame); return true; }'
    );
    await call('browser_tabs', {
      action: 'new',
      url: `http://127.0.0.1:${addr.port}`,
    });
    await call('browser_wait_for', { text: 'Ready to join' });
    await evaluate(
      '() => { document.querySelector("#nickname").value="SecondPilot"; document.querySelector("#connect").focus(); return true; }'
    );
    await call('browser_press_key', { key: 'Enter' });
    await call('browser_wait_for', { text: 'Playing' });
    const nativeBinary =
      process.env.XPILOT_CLIENT_BINARY ??
      resolve(dirname(binary), '../client/sdl/xpilot-infinity-sdl');
    native = spawn(
      'xvfb-run',
      [
        '-a',
        'env',
        'LIBGL_ALWAYS_SOFTWARE=1',
        'SDL_VIDEODRIVER=x11',
        'stdbuf',
        '-oL',
        nativeBinary,
        '-geometry',
        '800x600',
        '-join',
        '-name',
        'NativePilot',
        '-messagesToStdout',
        '2',
        `ws://127.0.0.1:${port}`,
      ],
      { detached: true }
    );
    native.stdout!.on('data', (chunk: Buffer) => {
      nativeLog += chunk.toString();
      if (nativeLog.includes('Game frame ready')) notifyNative();
    });
    native.stderr!.on('data', (chunk: Buffer) => {
      nativeLog += chunk.toString();
    });
    await nativeJoined;
    await evaluate(
      '() => { document.querySelector("#chat").value="こんにちは XPilot é"; document.querySelector("#chat-form").requestSubmit(); return true; }'
    );
    await call('browser_tabs', { action: 'select', index: 0 });
    await call('browser_wait_for', { text: 'こんにちは XPilot é' });
    await call('browser_take_screenshot', {
      filename: resolve(output, 'chat.png'),
      type: 'png',
    });
    await call('browser_stop_video', {});
    recording = false;
    expect((await stat(resolve(output, 'play.webm'))).size).toBeGreaterThan(
      5000
    );
    const previous = await evaluate(
      '() => Number(document.querySelector("#game").dataset.frame)'
    );
    for (const stream of streams) stream.destroy();
    await resumed;
    await evaluate(
      `async () => { while(document.querySelector("#connection").dataset.state !== 'playing' || Number(document.querySelector("#game").dataset.frame) <= ${previous}+20) await new Promise(requestAnimationFrame); return true; }`
    );
    expect(log).toContain('gameplay session resumed');
    await evaluate(
      '() => { document.querySelector("#disconnect").click(); return true; }'
    );
    await evaluate(
      'async () => { while(document.querySelector("#connection").dataset.state === "leaving") await new Promise(requestAnimationFrame); return true; }'
    );
    expect(
      await evaluate(
        '() => document.querySelector("#connection").dataset.state'
      )
    ).toBe('disconnected');
    await evaluate(
      '() => { document.querySelector("#join").requestSubmit(); return true; }'
    );
    await call('browser_wait_for', { text: 'Playing' });
    await evaluate(
      '() => { document.querySelector("#disconnect").click(); return true; }'
    );
    await call('browser_wait_for', { text: 'Disconnected' });
    expect(nativeLog).toContain('こんにちは XPilot é');
    if (native?.pid) process.kill(-native.pid, 'SIGTERM');
    expect(
      await evaluate(
        '() => Number(document.querySelector("#audio").dataset.active)'
      )
    ).toBe(0);
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
  } catch (error) {
    console.error(error);
    if (connected) {
      await writeFile(
        resolve(output, 'console.log'),
        await call('browser_console_messages', { level: 'debug' })
      );
      await writeFile(
        resolve(output, 'page.json'),
        JSON.stringify(
          await evaluate(
            '() => ({status:document.body.innerText, state:document.querySelector("#connection").dataset.state, game: {...document.querySelector("#game").dataset}})'
          )
        )
      );
      await call('browser_take_screenshot', {
        filename: resolve(output, 'failure.png'),
        type: 'png',
      });
    }
    throw error;
  } finally {
    await writeFile(resolve(output, 'server.log'), log);
    await writeFile(resolve(output, 'cleanup.txt'), 'client');
    if (recording) await call('browser_stop_video', {});
    if (connected) await client.close();
    await writeFile(resolve(output, 'cleanup.txt'), 'native');
    if (native?.pid && native.exitCode === null && native.signalCode === null)
      process.kill(-native.pid, 'SIGTERM');
    await writeFile(resolve(output, 'native-client.log'), nativeLog);
    const bridgeClosed = new Promise<void>((done) =>
      bridge.close(() => done())
    );
    for (const stream of streams) stream.destroy();
    await bridgeClosed;
    await writeFile(resolve(output, 'cleanup.txt'), 'http');
    for (const socket of httpStreams) socket.destroy();
    if (host && 'closeAllConnections' in host.httpServer)
      host.httpServer.closeAllConnections();
    if (host)
      await new Promise<void>((done) => host!.httpServer.close(() => done()));
    await writeFile(resolve(output, 'cleanup.txt'), 'server');
    if (server.exitCode === null && server.signalCode === null) {
      server.kill('SIGTERM');
      await once(server, 'exit');
    }
    await writeFile(resolve(output, 'server.log'), log);
  }
}, 90000);
