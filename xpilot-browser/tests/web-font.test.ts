// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { expect, test } from 'vitest';
import { normalizeWebFont } from '../web-font';

test('preserves nonempty font tables and recalculates SFNT checksums', () => {
  const source = new Uint8Array(80);
  const view = new DataView(source.buffer);
  view.setUint32(0, 0x00010000);
  view.setUint16(4, 3);
  for (const [index, tag, offset, length] of [
    [0, 'GDEF', 80, 0],
    [1, 'head', 60, 12],
    [2, 'test', 72, 5],
  ] as const) {
    source.set(new TextEncoder().encode(tag), 12 + index * 16);
    view.setUint32(12 + index * 16 + 8, offset);
    view.setUint32(12 + index * 16 + 12, length);
  }
  source.set([1, 2, 3, 4, 5], 72);
  const normalized = normalizeWebFont(source);
  const result = new DataView(normalized.buffer);
  expect(result.getUint16(4)).toBe(2);
  expect(result.getUint16(6)).toBe(32);
  expect(result.getUint16(8)).toBe(1);
  expect(result.getUint16(10)).toBe(0);
  expect(new TextDecoder().decode(normalized.slice(12, 16))).toBe('head');
  expect(new TextDecoder().decode(normalized.slice(28, 32))).toBe('test');
  expect(
    normalized.slice(result.getUint32(36), result.getUint32(36) + 5)
  ).toEqual(new Uint8Array([1, 2, 3, 4, 5]));
  let sum = 0;
  for (let offset = 0; offset < normalized.length; offset += 4)
    sum = (sum + result.getUint32(offset)) >>> 0;
  expect(sum).toBe(0xb1b0afba);
  expect(source[72]).toBe(1);
  expect(view.getUint32(68)).toBe(0);
  expect(normalizeWebFont(normalized)).toEqual(normalized);
  const padded = new Uint8Array(normalized.length + 4);
  padded.set(normalized);
  expect(normalizeWebFont(padded)).toEqual(padded);
});

test('rejects truncated font table directories', () => {
  const source = new Uint8Array(12);
  new DataView(source.buffer).setUint32(0, 0x00010000);
  new DataView(source.buffer).setUint16(4, 2);
  expect(() => normalizeWebFont(source)).toThrow();
});
