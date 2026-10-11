import { expect, it } from 'vitest';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createCore } from '../src/core';

it('loads a map, advances robots without waiting, and stops the WASM instance', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-world-'));
  const log: string[] = [];
  const core = await createCore({
    runtime: new URL('../dist/runtime/xpilot-server.mjs', import.meta.url),
    assets: fileURLToPath(new URL('../../lib', import.meta.url)),
    data,
    web: undefined,
    ready: () => {},
    log: (line: string) => log.push(line),
  });
  try {
    expect(
      core.start([
        '-map',
        'ndh.xp2',
        '+reportToMetaServer',
        '-noQuit',
        '-rawMode',
        '-framesPerSecond',
        '50',
        '-gameSpeed',
        '50',
        '-minRobots',
        '2',
        '-maxRobots',
        '2',
        '-listen',
        'udp:18351',
        '-serverHost',
        '127.0.0.1',
      ])
    ).toBe(0);
    expect(core.frames()).toBe(0);
    for (let frame = 0; frame < 120; frame++)
      expect(core.pump(true)).toBe(true);
    expect(core.frames()).toBe(120);
    expect(core.robots()).toBe(2);
    expect(log.some((line) => line.includes('Server runs at'))).toBe(true);
    core.stop();
    expect(core.pump(true)).toBe(false);
    expect(core.exitCode()).toBe(0);
  } finally {
    core.stop();
    await rm(data, { recursive: true, force: true });
  }
});
