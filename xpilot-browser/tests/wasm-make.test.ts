// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn, spawnSync } from 'node:child_process';
import {
  chmod,
  mkdtemp,
  mkdir,
  readFile,
  rm,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import { expect, test } from 'vitest';

test('two dependency builds sharing a prefix configure SDL only once', async () => {
  const scratch = await mkdtemp(join(tmpdir(), 'xpilot-wasm-make-'));
  const sdk = join(scratch, 'sdk');
  const bin = join(scratch, 'bin');
  const build = join(scratch, 'build');
  const prefix = join(build, 'prefix');
  const configured = join(scratch, 'configured');
  const release = join(scratch, 'release');
  const first = join(scratch, 'first');
  try {
    await mkdir(join(sdk, 'upstream/emscripten'), { recursive: true });
    await mkdir(bin);
    const emcmake = join(sdk, 'upstream/emscripten/emcmake');
    const cmake = join(bin, 'cmake');
    const flock = join(bin, 'flock');
    expect(spawnSync('mkfifo', [release]).status).toBe(0);
    await writeFile(
      emcmake,
      '#!/bin/sh\n' +
        'printf "CONFIGURE_START\\n"\n' +
        'if mkdir "$TEST_FIRST" 2>/dev/null; then\n' +
        '  IFS= read -r message < "$TEST_RELEASE"\n' +
        'fi\n' +
        'printf "configured\\n" >> "$TEST_BUILD_LOG"\n'
    );
    await writeFile(
      cmake,
      '#!/bin/sh\nif [ "$1" = "--install" ]; then mkdir -p "$TEST_PREFIX/lib"; : > "$TEST_PREFIX/lib/libSDL3.a"; fi\n'
    );
    await writeFile(
      flock,
      '#!/bin/sh\nprintf "LOCK_ENTER\\n"\nexec /usr/bin/flock "$@"\n'
    );
    await chmod(emcmake, 0o755);
    await chmod(cmake, 0o755);
    await chmod(flock, 0o755);
    const run = () => {
      const child = spawn(
        'make',
        [
          '-C',
          fileURLToPath(new URL('../wasm/', import.meta.url)),
          'dependencies',
          `EMSDK_ROOT=${sdk}`,
          `BUILD_DIR=${build}`,
          `PREFIX=${prefix}`,
        ],
        {
          env: {
            ...process.env,
            PATH: `${bin}:${process.env.PATH ?? ''}`,
            TEST_BUILD_LOG: configured,
            TEST_FIRST: first,
            TEST_PREFIX: prefix,
            TEST_RELEASE: release,
          },
        }
      );
      let output = '';
      const events: string[] = [];
      const listeners: Array<{
        names: string[];
        done: (event: string) => void;
      }> = [];
      createInterface({ input: child.stdout }).on('line', (line) => {
        output += `${line}\n`;
        if (line !== 'CONFIGURE_START' && line !== 'LOCK_ENTER') return;
        events.push(line);
        for (const listener of listeners) {
          if (listener.names.includes(line)) listener.done(line);
        }
      });
      child.stderr.on('data', (chunk: Buffer) => {
        output += chunk;
      });
      const done = new Promise<void>((resolve, reject) => {
        child.on('error', reject);
        child.on('exit', (code) => {
          if (code === 0) resolve();
          else reject(new Error(`make failed (${code}): ${output}`));
        });
      });
      const waitFor = (names: string[]) =>
        new Promise<string>((resolve) => {
          const seen = events.find((event) => names.includes(event));
          if (seen !== undefined) resolve(seen);
          else listeners.push({ names, done: resolve });
        });
      return { done, waitFor };
    };
    const initial = run();
    await initial.waitFor(['CONFIGURE_START']);
    const competing = run();
    await competing.waitFor(['LOCK_ENTER', 'CONFIGURE_START']);
    // The second invocation has reached the lock (or, without locking, CMake).
    const writer = spawn('sh', ['-c', 'printf "go\\n" > "$TEST_RELEASE"'], {
      env: { ...process.env, TEST_RELEASE: release },
    });
    await new Promise<void>((resolve, reject) => {
      writer.on('error', reject);
      writer.on('exit', (code) => {
        if (code === 0) resolve();
        else reject(new Error(`Could not release the first build (${code})`));
      });
    });
    await Promise.all([initial.done, competing.done]);
    expect((await readFile(configured, 'utf8')).trim().split('\n')).toEqual([
      'configured',
    ]);
  } finally {
    await rm(scratch, { recursive: true, force: true });
  }
}, 15000);
