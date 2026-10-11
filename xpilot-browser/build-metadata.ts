// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { buildCandidateVariables, runScriptOnceToText } from 'funcity';

/**
 * Generate the C build identity using the native version resolver.
 * @param root Absolute browser package path.
 * @returns Generated include directory.
 */
export const prepareBrowserMetadata = async (root: string): Promise<string> => {
  const { stdout } = await promisify(execFile)(
    resolve(root, '../config/resolve-build-metadata.sh'),
    [],
    {
      env: {
        ...process.env,
        SCREW_UP: resolve(root, 'node_modules/.bin/screw-up'),
      },
    }
  );
  const [version, commit] = stdout.trim().split('\n');
  const template = await readFile(resolve(root, 'wasm/version.h.fc'), 'utf8');
  const text = await runScriptOnceToText(template, {
    variables: buildCandidateVariables({ version, commit }),
    logs: [],
    sourceId: 'version.h.fc',
  });
  if (text === undefined)
    throw new Error('Could not generate browser build identity');
  const output = resolve(root, 'build/generated');
  await mkdir(output, { recursive: true });
  await writeFile(resolve(output, 'version.h'), text);
  return output;
};
