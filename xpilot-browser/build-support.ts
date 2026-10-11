// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { spawn } from 'node:child_process';
import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { availableParallelism } from 'node:os';
import { resolve } from 'node:path';
import { prepareEmsdk } from 'emsdk-env';
import { normalizeWebFont } from './web-font';

/** Emscripten version used by the browser runtime and its native dependencies. */
export const sdkVersion = '5.0.1';

/**
 * Build SDL with the same fixed SDK used by the emsdk-env Vite plugin.
 * @param root Absolute client workspace directory.
 * @returns Absolute install prefix containing the WASM SDL archive and headers.
 */
export const prepareBrowserDependencies = async (
  root: string
): Promise<string> => {
  const sdk = await prepareEmsdk({ targetVersion: sdkVersion });
  const build = resolve(root, 'build/wasm', sdkVersion);
  const prefix = resolve(build, 'prefix');
  await new Promise<void>((done, reject) => {
    const child = spawn(
      'make',
      [
        '-C',
        resolve(root, 'wasm'),
        'game-dependencies',
        `EMSDK_ROOT=${sdk}`,
        `BUILD_DIR=${build}`,
        `PREFIX=${prefix}`,
        `JOBS=${Math.min(8, availableParallelism())}`,
      ],
      { stdio: 'inherit' }
    );
    child.on('error', reject);
    child.on('exit', (code) => {
      if (code === 0) done();
      else reject(new Error(`Browser dependency build failed (${code})`));
    });
  });
  const fonts = resolve(root, 'public/fonts');
  await mkdir(fonts, { recursive: true });
  for (const name of ['VeraMoBd.ttf', 'FreeSansBoldOblique.ttf']) {
    const source = await readFile(resolve(root, '../lib/fonts', name));
    await writeFile(resolve(fonts, name), normalizeWebFont(source));
  }
  const assets = resolve(root, 'build/assets');
  await mkdir(assets, { recursive: true });
  for (const directory of ['fonts', 'textures', 'sound', 'maps'])
    await cp(resolve(root, '../lib', directory), resolve(assets, directory), {
      recursive: true,
      filter: (source) =>
        !source.endsWith('Makefile') &&
        !source.endsWith('Makefile.am') &&
        !source.endsWith('Makefile.in'),
    });
  for (const name of ['shipshapes.txt', 'defaults.txt'])
    await cp(resolve(root, '../lib', name), resolve(assets, name));
  return prefix;
};
