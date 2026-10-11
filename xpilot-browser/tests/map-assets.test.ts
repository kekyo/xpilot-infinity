// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { expect, test, vi } from 'vitest';
import { installMapAssets } from '../src/map-assets';

test('uses bundled map data without contacting an external URL', async () => {
  const fetcher = vi.fn();
  const fs = { stat: () => ({ size: 1 }), writeFile: vi.fn() };
  await installMapAssets(
    fs,
    'http://old.example/maps/ndh-1.3.xpd',
    'https://game.example/',
    new AbortController().signal,
    fetcher
  );
  expect(fetcher).not.toHaveBeenCalled();
});

test('loads missing XPD data from the content origin with a bounded size', async () => {
  const writeFile = vi.fn();
  const fs = {
    stat: () => {
      throw new Error('Missing');
    },
    writeFile,
  };
  const fetcher = vi.fn(async () => new Response(new Uint8Array([1, 2, 3])));
  const signal = new AbortController().signal;
  await installMapAssets(
    fs,
    'http://legacy.example/custom.xpd',
    'https://game.example/play/',
    signal,
    fetcher
  );
  expect(fetcher).toHaveBeenCalledWith('https://game.example/maps/custom.xpd', {
    signal,
    credentials: 'same-origin',
    redirect: 'error',
  });
  expect(writeFile).toHaveBeenCalledWith(
    '/assets/maps/custom.xpd',
    new Uint8Array([1, 2, 3])
  );
  for (const name of ['../%2fsecret.xpd', 'file.exe']) {
    await expect(
      installMapAssets(fs, name, 'https://game.example/', signal, fetcher)
    ).rejects.toThrow('map data name');
  }
  fetcher.mockImplementation(
    async () => new Response(new Uint8Array(8 * 1024 * 1024 + 1))
  );
  await expect(
    installMapAssets(fs, 'large.xpd', 'https://game.example/', signal, fetcher)
  ).rejects.toThrow('8 MiB');
});
