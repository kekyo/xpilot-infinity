import { expect, it } from 'vitest';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync } from 'node:zlib';
import { createCore } from '../src/core';

it('records UDP game frames and replays them in a fresh WASM instance', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-recording-'));
  const log: string[] = [];
  try {
    for (const mode of [1, 2]) {
      const core = await createCore({
        runtime: new URL('../dist/runtime/xpilot-server.mjs', import.meta.url),
        assets: fileURLToPath(new URL('../../lib', import.meta.url)),
        data,
        web: undefined,
        ready: () => {},
        log: (line) => log.push(line),
      });
      try {
        expect(
          await core.start([
            '-map',
            'ndh.xp2',
            '+reportMeta',
            '-noQuit',
            '-listen',
            'udp:18356',
            '-recordFileName',
            'game.xpr',
            '-recordMode',
            String(mode),
          ]),
          log.join('\n')
        ).toBe(0);
        for (let frame = 0; frame < 60; frame++)
          expect(core.pump(true)).toBe(true);
        expect(core.frames()).toBe(60);
      } finally {
        await core.stop();
      }
    }
    expect((await readFile(resolve(data, 'game.xpr'))).length).toBeGreaterThan(
      60
    );
  } finally {
    await rm(data, { recursive: true, force: true });
  }
});

it('uses host paths inside a defaults file for maps and saved state', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-config-'));
  const logs: string[] = [];
  const core = await createCore({
    runtime: new URL('../dist/runtime/xpilot-server.mjs', import.meta.url),
    assets: fileURLToPath(new URL('../../lib', import.meta.url)),
    data,
    web: undefined,
    ready: () => {},
    log: (line) => logs.push(line),
  });
  const map = resolve(data, 'custom map.xp2');
  const rank = resolve(data, 'saved ranking.xml');
  const config = resolve(data, 'server defaults.txt');
  await writeFile(
    map,
    await readFile(new URL('../../lib/maps/ndh.xp2', import.meta.url))
  );
  await writeFile(config, `mapFileName: ${map}\nrankFileName: ${rank}\n`);
  try {
    expect(
      await core.start([
        '-defaults',
        config,
        '-strictMap',
        '+reportMeta',
        '-noQuit',
        '-listen',
        'udp:18355',
      ]),
      logs.join('\n')
    ).toBe(0);
    expect(core.pump(true)).toBe(true);
    await core.stop();
    expect(await readFile(rank, 'utf8')).toContain(
      '<XPilotInfinityRank version="1.0">'
    );
  } finally {
    await core.stop();
    await rm(data, { recursive: true, force: true });
  }
});

it('loads a compressed custom map and retains rankings across WASM instances', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-data-'));
  const map = resolve(data, '日本語 map.xp2.gz');
  const logs: string[] = [];
  await writeFile(
    map,
    gzipSync(await readFile(new URL('../../lib/maps/ndh.xp2', import.meta.url)))
  );
  await writeFile(
    resolve(data, 'rankings.xml'),
    '<XPilotInfinityRank><Players><Player name="SavedPilot" user="saved" host="localhost" score="42" /></Players></XPilotInfinityRank>'
  );
  try {
    for (let run = 0; run < 2; run++) {
      const core = await createCore({
        runtime: new URL('../dist/runtime/xpilot-server.mjs', import.meta.url),
        assets: fileURLToPath(new URL('../../lib', import.meta.url)),
        data,
        web: undefined,
        ready: () => {},
        log: (line) => logs.push(line),
      });
      try {
        expect(
          await core.start([
            '-map',
            map,
            '-strictMap',
            '+reportMeta',
            '-noQuit',
            '-listen',
            'udp:18353',
            '-serverHost',
            '127.0.0.1',
            '-rankFileName',
            'rankings.xml',
          ]),
          logs.join('\n')
        ).toBe(0);
        for (let i = 0; i < 10; i++) core.pump(true);
      } finally {
        await core.stop();
      }
      const saved = await readFile(resolve(data, 'rankings.xml'), 'utf8');
      expect(saved).toContain('SavedPilot');
      expect(saved).toContain('score="42.00"');
    }
  } finally {
    await rm(data, { recursive: true, force: true });
  }
});

it('resolves a configured hostname through the host before binding', async () => {
  const data = await mkdtemp(resolve(tmpdir(), 'xpilot-node-dns-'));
  const logs: string[] = [];
  const core = await createCore({
    runtime: new URL('../dist/runtime/xpilot-server.mjs', import.meta.url),
    assets: fileURLToPath(new URL('../../lib', import.meta.url)),
    data,
    web: undefined,
    ready: () => {},
    log: (line) => logs.push(line),
  });
  try {
    expect(
      await core.start([
        '-map',
        'ndh.xp2',
        '+reportMeta',
        '-noQuit',
        '-serverHost',
        'localhost',
        '-listen',
        'udp:18354',
      ]),
      logs.join('\n')
    ).toBe(0);
    expect(core.pump(true)).toBe(true);
  } finally {
    await core.stop();
    await rm(data, { recursive: true, force: true });
  }
});
