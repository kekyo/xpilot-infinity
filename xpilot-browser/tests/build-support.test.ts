// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { EventEmitter } from 'node:events';
import { expect, test, vi } from 'vitest';

const calls = vi.hoisted(() => ({
  command: '',
  args: [] as string[],
}));

vi.mock('emsdk-env', () => ({
  prepareEmsdk: async () => '/cache/emsdk/5.0.1',
}));
vi.mock('node:child_process', () => ({
  spawn: (command: string, args: string[]) => {
    calls.command = command;
    calls.args = args;
    const child = new EventEmitter();
    queueMicrotask(() => child.emit('exit', 0));
    return child;
  },
}));
vi.mock('node:fs/promises', () => ({
  mkdir: async () => undefined,
  readFile: async () => new Uint8Array(12),
  writeFile: async () => undefined,
}));
vi.mock('../web-font', () => ({
  normalizeWebFont: (bytes: Uint8Array) => bytes,
}));

test('builds and installs SDL inside the selected SDK version directory', async () => {
  const { prepareBrowserDependencies } = await import('../build-support');
  const prefix = await prepareBrowserDependencies('/project/xpilot-browser');
  expect(prefix).toBe('/project/xpilot-browser/build/wasm/5.0.1/prefix');
  expect(calls.command).toBe('make');
  expect(calls.args).toContain(
    'BUILD_DIR=/project/xpilot-browser/build/wasm/5.0.1'
  );
  expect(calls.args).toContain(`PREFIX=${prefix}`);
});
