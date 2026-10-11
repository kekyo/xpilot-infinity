// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { mkdir, readFile, rm, stat, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import type { CallToolResult } from '@modelcontextprotocol/sdk/types.js';
import { PNG } from 'pngjs';
import { createServer, preview } from 'vite';
import { expect, test } from 'vitest';

test.each([1, 2])(
  'renders the WASM scene and responds to input at pixel ratio %i',
  async (scale) => {
    const output = resolve(`test-results/browser-driver-${scale}`);
    await rm(output, { recursive: true, force: true });
    await mkdir(output, { recursive: true });
    const server = await preview({
      configFile: false,
      preview: { host: '127.0.0.1', port: 0 },
    });
    const require = createRequire(import.meta.url);
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
            ...(process.env.XPILOT_TEST_BROWSER === undefined
              ? {}
              : {
                  executablePath: process.env.XPILOT_TEST_BROWSER,
                }),
            args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
          },
          contextOptions: {
            viewport: { width: 1000, height: 800 },
            deviceScaleFactor: scale,
          },
        },
        capabilities: ['core', 'devtools'],
        outputDir: output,
        timeouts: { action: 15000, navigation: 30000 },
      })
    );
    const client = new Client({
      name: 'xpilot-browser-tests',
      version: '1.0.0',
    });
    const transport = new StdioClientTransport({
      command: process.execPath,
      args: [
        resolve(
          dirname(require.resolve('@playwright/mcp/package.json')),
          'cli.js'
        ),
        '--config',
        configPath,
      ],
      cwd: process.cwd(),
    });
    let recording = false;
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
    const evaluate = async (expression: string) => {
      const response = await call('browser_evaluate', { function: expression });
      const json = response.split('### Result\n')[1]?.split('\n###')[0];
      if (json === undefined) throw new Error(response);
      return JSON.parse(json);
    };

    try {
      await client.connect(transport);
      const address = server.httpServer.address();
      if (address === null || typeof address === 'string')
        throw new Error('No HTTP port');
      await call('browser_navigate', {
        url: `http://127.0.0.1:${address.port}`,
      });
      expect(
        await evaluate('() => Boolean(document.querySelector("#game"))')
      ).toBe(true);
      await call('browser_start_video', {
        filename: resolve(output, 'input.webm'),
      });
      recording = true;
      await call('browser_wait_for', { text: 'Rendering' });
      expect(
        await evaluate('() => document.querySelector("#status").textContent')
      ).toBe('Rendering ready');
      expect(
        await evaluate(
          '() => Boolean(document.querySelector("#game").getContext("webgl2"))'
        )
      ).toBe(true);
      await call('browser_take_screenshot', {
        type: 'png',
        filename: resolve(output, 'initial.png'),
        target: '#game',
      });
      const initial = PNG.sync.read(
        await readFile(resolve(output, 'initial.png'))
      );
      // Scene pixels, independent of the browser's DOM status and driver counters.
      const colors = new Set<string>();
      for (let i = 0; i < initial.data.length; i += 4) {
        colors.add(
          `${initial.data[i]},${initial.data[i + 1]},${initial.data[i + 2]}`
        );
      }
      expect(colors.size).toBeGreaterThan(20);
      expect(colors.has('10,20,40')).toBe(true);
      expect(colors.has('35,135,105')).toBe(true);
      expect(colors.has('255,82,108')).toBe(true);
      expect(colors.has('255,192,36')).toBe(true);
      expect(colors.has('32,125,233')).toBe(true);

      const before = await evaluate(
        '() => Number(document.querySelector("#game").dataset.sceneX)'
      );
      await call('browser_evaluate', {
        function: '() => document.querySelector("#game").focus()',
      });
      await call('browser_press_key', { key: 'ArrowRight' });
      const after = await evaluate(
        '() => Number(document.querySelector("#game").dataset.sceneX)'
      );
      expect(after).toBeGreaterThan(before);
      await call('browser_take_screenshot', {
        type: 'png',
        filename: resolve(output, 'moved.png'),
        target: '#game',
      });
      const moved = PNG.sync.read(await readFile(resolve(output, 'moved.png')));
      expect(moved.data.equals(initial.data)).toBe(false);

      const width = await evaluate(
        '() => document.querySelector("#game").width'
      );
      expect(width).toBe(960 * scale);
      await call('browser_resize', { width: 720, height: 600 });
      const resized = await evaluate(
        `async () => {
          const canvas = document.querySelector('#game');
          const viewport = document.querySelector('#viewport');
          const deadline = performance.now() + 15000;
          while (canvas.width !== Math.floor(viewport.clientWidth) * devicePixelRatio ||
                 canvas.height !== Math.floor(viewport.clientHeight) * devicePixelRatio) {
            if (performance.now() > deadline) throw new Error('Canvas did not follow the resized viewport');
            await new Promise(requestAnimationFrame);
          }
          return { width: canvas.width, height: canvas.height };
        }`
      );
      expect(resized.width).toBeLessThan(width);
      expect(resized.height).toBeGreaterThan(0);
      await call('browser_take_screenshot', {
        type: 'png',
        filename: resolve(output, 'resized.png'),
        target: '#game',
      });
      await evaluate(`() => {
        window.dispatchEvent(new PageTransitionEvent('pagehide', { persisted: true }));
        window.dispatchEvent(new PageTransitionEvent('pageshow', { persisted: true }));
        return true;
      }`);
      const restoredX = await evaluate(
        '() => Number(document.querySelector("#game").dataset.sceneX)'
      );
      await call('browser_press_key', { key: 'ArrowRight' });
      expect(
        await evaluate(
          '() => Number(document.querySelector("#game").dataset.sceneX)'
        )
      ).toBeGreaterThan(restoredX);
      const consoleErrors = await call('browser_console_messages', {
        level: 'error',
      });
      expect(consoleErrors).not.toMatch(/\[ERROR\]/u);
      await call('browser_stop_video', {});
      recording = false;
      expect((await stat(resolve(output, 'input.webm'))).size).toBeGreaterThan(
        1000
      );
      // Compare the actual browser text service against an unclipped reference.
      const dev = await createServer({
        configFile: false,
        server: { host: '127.0.0.1', port: 0 },
      });
      try {
        await dev.listen();
        const devAddress = dev.httpServer!.address();
        if (devAddress === null || typeof devAddress === 'string')
          throw new Error('No development HTTP port');
        await call('browser_navigate', {
          url: `http://127.0.0.1:${devAddress.port}`,
        });
        const result = await evaluate(`async () => {
        const { createBrowserTextProvider } = await import('/src/browser-text.ts');
        const provider = await createBrowserTextProvider('/fonts/VeraMoBd.ttf', '/fonts/FreeSansBoldOblique.ttf');
        try {
          const samples = [];
          for (const scale of [1, 1.25, 2]) {
            const request = { text: 'fTjÅ é 日本語', size: 32, face: 'oblique', scale };
            const raster = provider.rasterize(request);
            const canvas = document.createElement('canvas');
            canvas.width = Math.ceil(raster.advance * scale) + 256;
            canvas.height = 256;
            const context = canvas.getContext('2d');
            context.font = 'oblique 700 32px "XPilotOblique", sans-serif';
            context.textBaseline = 'alphabetic';
            context.textAlign = 'left';
            context.direction = 'ltr';
            context.fillStyle = 'white';
            context.setTransform(scale, 0, 0, scale, 128, 128);
            context.fillText(request.text, 0, 0);
            const reference = context.getImageData(0, 0, canvas.width, canvas.height).data;
            let ink = 0;
            let clipped = 0;
            let differences = 0;
            for (let y = 0; y < canvas.height; y++) {
              for (let x = 0; x < canvas.width; x++) {
                const alpha = reference[(y * canvas.width + x) * 4 + 3];
                const rx = x - 128 + raster.baselineX;
                const ry = y - 128 + raster.baselineY;
                const inside = rx >= 0 && ry >= 0 && rx < raster.pixelWidth && ry < raster.pixelHeight;
                if (alpha !== 0) { ink++; if (!inside) clipped++; }
                const actual = inside ? raster.pixels[(ry * raster.pixelWidth + rx) * 4 + 3] : 0;
                if (actual !== alpha) differences++;
              }
            }
            samples.push({ ink, clipped, differences, cached: provider.rasterize(request) === raster });
          }
          const first = { text: 'cache', size: 24, face: 'mono', scale: 1 };
          const original = provider.rasterize(first);
          for (let i = 0; i < 130; i++) provider.rasterize({ ...first, text: String(i) });
          return { samples, evicted: provider.rasterize(first) !== original };
        } finally { provider.dispose(); }
      }`);
        for (const sample of result.samples) {
          expect(sample.ink).toBeGreaterThan(100);
          expect(sample.clipped).toBe(0);
          expect(sample.differences).toBe(0);
          expect(sample.cached).toBe(true);
        }
        expect(result.evicted).toBe(true);
      } finally {
        await dev.close();
      }
    } catch (error) {
      await writeFile(
        resolve(output, 'failure.txt'),
        await call('browser_snapshot', {})
      );
      await call('browser_take_screenshot', {
        filename: resolve(output, 'failure.png'),
      });
      throw error;
    } finally {
      if (recording) await call('browser_stop_video', {});
      await client.close();
      await server.close();
    }
  },
  120000
);
