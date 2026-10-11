// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** The public Emscripten filesystem operations used by map installation. */
export interface MapAssetFiles {
  /** Look up a bundled file; throw when it does not exist. */
  stat: (path: string) => { size: number };
  /** Install a complete file in the existing assets/maps directory. */
  writeFile: (path: string, data: Uint8Array) => void;
}

/**
 * Install missing map textures from the page origin before C map initialization.
 * @param files The game instance's virtual filesystem.
 * @param dataURL Map metadata URL, used only to select an XPD basename.
 * @param pageURL Content origin; remote map URLs never become fetch destinations.
 * @param signal Cancellation when joining is cancelled or the page is left.
 * @param fetcher HTTP implementation, defaulting to the browser fetch API.
 */
export const installMapAssets = async (
  files: MapAssetFiles,
  dataURL: string,
  pageURL: string,
  signal: AbortSignal,
  fetcher: typeof fetch = fetch
): Promise<void> => {
  if (!dataURL) return;
  const name = decodeURIComponent(
    new URL(dataURL, pageURL).pathname.split('/').pop()!
  );
  if (!/^[a-zA-Z0-9][a-zA-Z0-9_.-]{0,119}\.xpd$/.test(name))
    throw new Error('Invalid map data name');
  const destination = `/assets/maps/${name}`;
  try {
    if (files.stat(destination).size > 0) return;
  } catch {
    /* Not bundled. */
  }
  const response = await fetcher(new URL(`/maps/${name}`, pageURL).href, {
    signal,
    credentials: 'same-origin',
    redirect: 'error',
  });
  if (!response.ok || !response.body)
    throw new Error(`Map data unavailable: ${name} (${response.status})`);
  const maximum = 8 * 1024 * 1024;
  const reader = response.body.getReader();
  const chunks: Uint8Array[] = [];
  let length = 0;
  try {
    for (;;) {
      const result = await reader.read();
      if (result.done) break;
      length += result.value.length;
      if (length > maximum) throw new Error('Map data exceeds 8 MiB');
      chunks.push(result.value);
    }
  } finally {
    await reader.cancel();
    reader.releaseLock();
  }
  const data = new Uint8Array(length);
  let offset = 0;
  for (const chunk of chunks) {
    data.set(chunk, offset);
    offset += chunk.length;
  }
  files.writeFile(destination, data);
};
