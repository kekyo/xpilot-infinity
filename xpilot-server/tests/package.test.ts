import { expect, it } from 'vitest';
import { execFile, spawn } from 'node:child_process';
import { promisify } from 'node:util';
import { once } from 'node:events';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';
import { BoundSocket } from 'node:net';

it('installs a self-contained release without dev dependencies or install scripts', async () => {
  const root = fileURLToPath(new URL('..', import.meta.url));
  const exec = promisify(execFile);
  await exec(process.execPath, ['pack.ts'], { cwd: root });
  const info = JSON.parse(
    await readFile(resolve(root, 'dist/build-info.json'), 'utf8')
  );
  const install = await mkdtemp(resolve(tmpdir(), 'xpilot-npm-installed-'));
  try {
    await exec(
      'npm',
      [
        'install',
        '--engine-strict',
        '--omit=dev',
        '--ignore-scripts',
        '--no-audit',
        '--no-fund',
        resolve(root, `artifacts/xpilot-infinity-${info.version}.tgz`),
      ],
      { cwd: install }
    );
    const packageRoot = resolve(install, 'node_modules/xpilot-infinity');
    const manifest = JSON.parse(
      await readFile(resolve(packageRoot, 'package.json'), 'utf8')
    );
    expect(manifest.version).toBe(info.version);
    expect(manifest.gitHead).toBe(info.commit);
    expect(manifest.devDependencies).toBeUndefined();
    const cli = resolve(packageRoot, 'dist/cli.mjs');
    const version = await exec(process.execPath, [cli, '--version']);
    expect(version.stdout.trim()).toBe(
      `XPilot Infinity [${info.version}-${info.commit}]`
    );
    const reservation = new BoundSocket();
    const port = reservation.address().port;
    reservation.close();
    // Runtime has no compiler, Emscripten, native server, or npm on PATH.
    const server = spawn(
      process.execPath,
      [
        cli,
        '--data-dir',
        resolve(install, 'data'),
        '-listen',
        `websocket:${port}`,
      ],
      { env: { ...process.env, PATH: dirname(process.execPath) } }
    );
    let log = '';
    try {
      await new Promise<void>((done, reject) => {
        server.stdout.on('data', (chunk) => {
          log += chunk.toString();
          if (log.includes('Server runs at')) done();
        });
        server.stderr.on('data', (chunk) => {
          log += chunk.toString();
        });
        server.once('error', reject);
        server.once('exit', (code) =>
          reject(new Error(`Server exited ${code}: ${log}`))
        );
      });
      const response = await fetch(`http://127.0.0.1:${port}/`);
      expect(response.status).toBe(200);
      expect(await response.text()).toContain('canvas');
      const browserInfo = await fetch(
        `http://127.0.0.1:${port}/build-info.json`
      );
      expect(await browserInfo.json()).toEqual(info);
    } finally {
      if (server.exitCode === null && server.signalCode === null) {
        const closed = once(server, 'exit');
        server.kill('SIGTERM');
        expect((await closed)[0], log).toBe(0);
      }
    }
  } finally {
    await rm(install, { recursive: true, force: true });
  }
}, 60000);
