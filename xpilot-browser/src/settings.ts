// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** Browser preferences and the existing C client's saved option file. */
export interface BrowserSettings {
  /** Player name used for the next admission. */
  nickname: string;
  /** Whether effects are muted. */
  muted: boolean;
  /** UTF-8 xpilotrc contents, installed before C option parsing. */
  config: string;
}

const open = async (): Promise<IDBDatabase> =>
  await new Promise((resolve, reject) => {
    const request = indexedDB.open('xpilot-infinity', 1);
    request.onupgradeneeded = () =>
      request.result.createObjectStore('settings');
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
    request.onblocked = () =>
      reject(new Error('Close other XPilot pages to update settings storage'));
  });

/** Read this origin's preferences; storage failures are reported to the caller. */
export const loadSettings = async (): Promise<BrowserSettings> => {
  const db = await open();
  try {
    const value: unknown = await new Promise((resolve, reject) => {
      const request = db
        .transaction('settings')
        .objectStore('settings')
        .get('player');
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
    if (
      value &&
      typeof value === 'object' &&
      'nickname' in value &&
      typeof value.nickname === 'string' &&
      value.nickname.length <= 24 &&
      'muted' in value &&
      typeof value.muted === 'boolean' &&
      'config' in value &&
      typeof value.config === 'string' &&
      value.config.length <= 512 * 1024
    )
      return {
        nickname: value.nickname,
        muted: value.muted,
        config: value.config,
      };
    return { nickname: 'BrowserPilot', muted: false, config: '' };
  } finally {
    db.close();
  }
};

/**
 * Commit preferences before reporting success; does not depend on page exit.
 * @param value Complete preferences to replace the previous saved value.
 */
export const saveSettings = async (value: BrowserSettings): Promise<void> => {
  if (value.config.length > 512 * 1024)
    throw new Error('Settings exceed 512 KiB');
  const db = await open();
  try {
    await new Promise<void>((resolve, reject) => {
      const transaction = db.transaction('settings', 'readwrite');
      transaction.oncomplete = () => resolve();
      transaction.onabort = () =>
        reject(transaction.error ?? new Error('Settings transaction aborted'));
      transaction.onerror = () => reject(transaction.error);
      transaction.objectStore('settings').put(value, 'player');
    });
  } finally {
    db.close();
  }
};
