// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { execFile } from 'node:child_process';
import {
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { expect, test, vi } from 'vitest';
import { prepareBrowserMetadata } from '../build-metadata';

test('generates the build identity without a parent npm workspace', async () => {
  const scratch = await mkdtemp(join(tmpdir(), 'xpilot-standalone-metadata-'));
  const root = join(scratch, 'xpilot-infinity');
  const browser = join(root, 'xpilot-browser');
  const run = promisify(execFile);
  vi.stubEnv('XPILOT_VERSION', '');
  vi.stubEnv('XPILOT_COMMIT_ID', '');
  try {
    await mkdir(join(root, 'config'), { recursive: true });
    await mkdir(join(browser, 'wasm'), { recursive: true });
    await copyFile(
      fileURLToPath(
        new URL('../../config/resolve-build-metadata.sh', import.meta.url)
      ),
      join(root, 'config/resolve-build-metadata.sh')
    );
    await copyFile(
      fileURLToPath(new URL('../wasm/version.h.fc', import.meta.url)),
      join(browser, 'wasm/version.h.fc')
    );
    await writeFile(
      join(root, '.gitignore'),
      'xpilot-browser/node_modules/\nxpilot-browser/build/\n'
    );
    await symlink(
      fileURLToPath(new URL('../node_modules', import.meta.url)),
      join(browser, 'node_modules'),
      'dir'
    );
    await run('git', ['init', '-q'], { cwd: root });
    await run('git', ['add', '.'], { cwd: root });
    await run(
      'git',
      [
        '-c',
        'user.name=Build test',
        '-c',
        'user.email=build@example.invalid',
        '-c',
        'commit.gpgsign=false',
        'commit',
        '-qm',
        'Create standalone source fixture',
      ],
      { cwd: root }
    );
    await run('git', ['tag', '1.2.3'], { cwd: root });
    const { stdout: commit } = await run('git', ['rev-parse', 'HEAD'], {
      cwd: root,
    });

    const { stdout: metadata } = await run(
      join(root, 'config/resolve-build-metadata.sh'),
      [],
      {
        env: {
          ...process.env,
          SCREW_UP: join(browser, 'node_modules/.bin/screw-up'),
        },
      }
    );
    expect(metadata.trim().split('\n')).toEqual(['1.2.3', commit.trim()]);

    const generated = await prepareBrowserMetadata(browser);
    const header = await readFile(join(generated, 'version.h'), 'utf8');
    expect(header).toContain('#define VERSION "1.2.3"');
    expect(header).toContain(`#define XPILOT_COMMIT_ID "${commit.trim()}"`);
  } finally {
    vi.unstubAllEnvs();
    await rm(scratch, { recursive: true, force: true });
  }
});
