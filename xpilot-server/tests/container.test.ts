import { expect, it } from 'vitest';
import { execFile, spawn } from 'node:child_process';
import { promisify } from 'node:util';
import { once } from 'node:events';
import { randomUUID } from 'node:crypto';

it.runIf(process.env.XPILOT_CONTAINER_IMAGE !== undefined)(
  'serves the bundled browser and WebSocket from a read-only Node container and retains data',
  async () => {
    const engine = process.env.CONTAINER_ENGINE ?? 'podman';
    const image = process.env.XPILOT_CONTAINER_IMAGE!;
    const platform =
      process.env.XPILOT_CONTAINER_PLATFORM ??
      `linux/${process.arch === 'x64' ? 'amd64' : process.arch}`;
    const name = `xpilot-node-check-${randomUUID()}`;
    const volume = `${name}-data`;
    const exec = promisify(execFile);
    const run = async (args: string[]) =>
      (
        await exec(engine, args, {
          timeout: 180000,
          maxBuffer: 4 * 1024 * 1024,
        })
      ).stdout.trim();
    const inspect = JSON.parse(await run(['image', 'inspect', image]))[0];
    expect(inspect.Architecture).toBe(platform.split('/')[1]);
    expect(inspect.Config.User).toBe('10001:10001');
    const runtime = JSON.parse(
      await run([
        'run',
        '--rm',
        '--platform',
        platform,
        '--entrypoint',
        'node',
        image,
        '-p',
        'JSON.stringify({arch:process.arch,net:typeof require("node:net").BoundSocket})',
      ])
    );
    expect(runtime).toEqual({
      arch: platform.endsWith('amd64') ? 'x64' : 'arm64',
      net: 'function',
    });
    const version = await run([
      'run',
      '--rm',
      '--platform',
      platform,
      image,
      '--version',
    ]);
    expect(version).toBe(
      `XPilot Infinity [${inspect.Config.Labels['org.opencontainers.image.version']}-${inspect.Config.Labels['org.opencontainers.image.revision']}]`
    );
    await run(['volume', 'create', volume]);
    try {
      // Seed a real ranking and read it back after two orderly server shutdowns.
      await run([
        'run',
        '--rm',
        '--platform',
        platform,
        '-v',
        `${volume}:/var/lib/xpilot-infinity-server`,
        '--entrypoint',
        'node',
        image,
        '-e',
        'require("node:fs").writeFileSync("/var/lib/xpilot-infinity-server/rankings.xml", \'<XPilotInfinityRank><Players><Player name="SavedPilot" user="saved" host="localhost" score="42" /></Players></XPilotInfinityRank>\')',
      ]);
      for (let attempt = 0; attempt < 2; attempt++) {
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
          `${volume}:/var/lib/xpilot-infinity-server`,
          image,
          '-map',
          'ndh.xp2',
          '-rankFileName',
          'rankings.xml',
        ]);
        const logs = spawn(engine, ['logs', '--follow', name]);
        let output = '';
        try {
          await new Promise<void>((done, reject) => {
            const collect = (chunk: Buffer) => {
              output += chunk.toString();
              if (output.includes('Server runs at')) done();
            };
            logs.stdout.on('data', collect);
            logs.stderr.on('data', collect);
            logs.once('error', reject);
            logs.once('exit', (code) =>
              reject(new Error(`Container exited (${code}): ${output}`))
            );
          });
        } finally {
          const closed = once(logs, 'close');
          logs.kill('SIGTERM');
          await closed;
        }
        for (const transport of [
          'udp on port 15345',
          'tcp on port 15346',
          'websocket on port 15347',
        ])
          expect(output).toContain(transport);
        const mapping = await run(['port', name]);
        expect(mapping.split('\n')).toHaveLength(1);
        expect(mapping).toMatch(/^15347\/tcp -> 127\.0\.0\.1:\d+$/);
        const port = mapping.split(':').at(-1)!;
        const url = `http://127.0.0.1:${port}`;
        const page = await fetch(url);
        expect(page.status).toBe(200);
        expect(await page.text()).toContain('canvas');
        const wasm = await fetch(`${url}/runtime/xpilot-game.wasm`);
        expect(wasm.headers.get('content-type')).toContain('application/wasm');
        expect((await wasm.arrayBuffer()).byteLength).toBeGreaterThan(1000);
        const info = await (await fetch(`${url}/build-info.json`)).json();
        expect(info).toEqual({
          version: inspect.Config.Labels['org.opencontainers.image.version'],
          commit: inspect.Config.Labels['org.opencontainers.image.revision'],
        });
        const socket = new WebSocket(
          `ws://127.0.0.1:${port}/xpilot`,
          'xpilot-infinity-v1'
        );
        try {
          await new Promise<void>((done, reject) => {
            socket.addEventListener('open', () => done(), { once: true });
            socket.addEventListener(
              'error',
              () => reject(new Error('WebSocket handshake failed')),
              { once: true }
            );
          });
          expect(socket.protocol).toBe('xpilot-infinity-v1');
        } finally {
          socket.close();
        }
        await run(['stop', '--time', '60', name]);
        expect(
          await run(['inspect', '--format', '{{.State.ExitCode}}', name])
        ).toBe('0');
        await run(['rm', name]);
        const ranking = await run([
          'run',
          '--rm',
          '--platform',
          platform,
          '--read-only',
          '-v',
          `${volume}:/data:ro`,
          '--entrypoint',
          'node',
          image,
          '-e',
          'process.stdout.write(require("node:fs").readFileSync("/data/rankings.xml","utf8"))',
        ]);
        expect(ranking).toContain('SavedPilot');
        expect(ranking).toContain('score="42.00"');
      }
    } catch (error) {
      const result = await exec(engine, ['logs', name]);
      console.error(result.stdout, result.stderr);
      throw error;
    } finally {
      await run(['rm', '--force', '--ignore', name]);
      await run(['volume', 'rm', '--force', volume]);
    }
  },
  300000
);
