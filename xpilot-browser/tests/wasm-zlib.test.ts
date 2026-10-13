// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { execFile } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { prepareEmsdk } from 'emsdk-env';
import { expect, test } from 'vitest';

test.each([
  ['browser', '5.0.1', '../wasm/'],
  ['server', '6.0.12', '../../xpilot-server/wasm/'],
])(
  '%s builds and runs zlib from a clean prefix',
  async (_name, version, makeDirectory) => {
    const scratch = await mkdtemp(join(tmpdir(), 'xpilot-wasm-zlib-'));
    const run = promisify(execFile);
    try {
      const sdk = await prepareEmsdk({ targetVersion: version });
      const prefix = join(scratch, 'prefix');
      const library = join(prefix, 'lib/libz.a');
      const env = { ...process.env, EM_CONFIG: join(sdk, '.emscripten') };
      const built = await run(
        'make',
        [
          '-C',
          fileURLToPath(new URL(makeDirectory, import.meta.url)),
          library,
          `EMSDK_ROOT=${sdk}`,
          `BUILD_DIR=${scratch}`,
          `PREFIX=${prefix}`,
          'JOBS=4',
        ],
        { env, maxBuffer: 8 * 1024 * 1024 }
      );
      expect(built.stderr).not.toMatch(/warning[: ]/i);
      const driver = join(scratch, 'example.js');
      await run(
        join(sdk, 'upstream/emscripten/emcc'),
        [
          '-std=c99',
          '-msimd128',
          '-Wno-deprecated-non-prototype',
          '-I',
          join(prefix, 'include'),
          fileURLToPath(
            new URL('../../vendor/zlib/test/example.c', import.meta.url)
          ),
          library,
          '-sENVIRONMENT=node',
          '-o',
          driver,
        ],
        { env }
      );
      const { stdout } = await run(process.execPath, [driver], {
        cwd: scratch,
      });
      expect(stdout).toContain('large_inflate(): OK');
      expect(stdout).toContain('inflate with dictionary: hello, hello!');
    } finally {
      await rm(scratch, { recursive: true, force: true });
    }
  },
  120000
);
