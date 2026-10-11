// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { test, expect } from 'vitest';
import { initializeWithListeners } from '../src/runtime-listeners';

test('removes every runtime listener on disposal, retaining page listeners across rejoin', () => {
  const page = new EventTarget();
  let pageEvents = 0,
    gameEvents = 0;
  page.addEventListener('input', () => pageEvents++);
  const original = page.addEventListener;
  for (let i = 0; i < 3; i++) {
    const scope = initializeWithListeners([page], () => {
      page.addEventListener('input', () => gameEvents++);
      return i;
    });
    expect(scope.value).toBe(i);
    expect(page.addEventListener).toBe(original);
    page.dispatchEvent(new Event('input'));
    expect(gameEvents).toBe(i + 1);
    scope.dispose();
    page.dispatchEvent(new Event('input'));
    expect(gameEvents).toBe(i + 1);
  }
  expect(pageEvents).toBe(6);
});

test('also releases partially initialized runtime listeners when initialization throws', () => {
  const page = new EventTarget();
  let events = 0;
  expect(() =>
    initializeWithListeners([page], () => {
      page.addEventListener('input', () => events++);
      throw new Error('initialization failed');
    })
  ).toThrow('initialization failed');
  page.dispatchEvent(new Event('input'));
  expect(events).toBe(0);
});
