// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { expect, test } from 'vitest';

test.each(['../vite.config.ts', '../../xpilot-server/vite.config.ts'])(
  'loads %s with native Node.js module resolution',
  async (path) => {
    const { stdout } = await promisify(execFile)(process.execPath, [
      '--input-type=module',
      '-e',
      'const config = await import(process.argv[1]); console.log(typeof config.default);',
      new URL(path, import.meta.url).href,
    ]);
    expect(stdout.trim()).toBe('function');
  }
);
