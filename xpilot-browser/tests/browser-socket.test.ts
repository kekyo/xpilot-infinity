// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { expect, test } from 'vitest';
import {
  createBrowserSocket,
  sameOriginEndpoint,
  type BrowserSocketPeer,
} from '../src/browser-socket';

const peer = () => {
  const events = new EventTarget();
  const sent: Uint8Array[] = [];
  const socket = Object.assign(events, {
    readyState: 0,
    bufferedAmount: 0,
    binaryType: 'blob' as BinaryType,
    protocol: 'xpilot-infinity-v1',
    send: (value: Uint8Array) => {
      sent.push(value.slice());
    },
    close: () => {
      socket.readyState = 3;
    },
  });
  return {
    socket,
    sent,
    open: () => {
      socket.readyState = 1;
      socket.dispatchEvent(new Event('open'));
    },
    message: (data: unknown) =>
      socket.dispatchEvent(new MessageEvent('message', { data })),
  };
};

test('uses only the document origin and the XPilot websocket resource', () => {
  expect(sameOriginEndpoint('https://game.example:8443/play/?host=evil')).toBe(
    'wss://game.example:8443/xpilot'
  );
  expect(sameOriginEndpoint('http://localhost:8080/')).toBe(
    'ws://localhost:8080/xpilot'
  );
  expect(() => sameOriginEndpoint('file:///index.html')).toThrow();
});

test('preserves binary record boundaries and drains received records before close', () => {
  const p = peer();
  const socket = createBrowserSocket(p.socket as BrowserSocketPeer);
  expect(socket.send(new Uint8Array([7]), false)).toBe(0);
  p.open();
  expect(p.socket.binaryType).toBe('arraybuffer');
  expect(socket.send(new Uint8Array([7, 8]), false)).toBe(1);
  expect(p.sent).toEqual([new Uint8Array([7, 8])]);
  p.message(new Uint8Array([1, 2]).buffer);
  p.message(new Uint8Array([3]).buffer);
  p.socket.readyState = 3;
  p.socket.dispatchEvent(new Event('close'));
  expect(socket.receive()).toEqual(new Uint8Array([1, 2]));
  expect(socket.receive()).toEqual(new Uint8Array([3]));
  expect(socket.receive()).toBe(-1);
});

test('required records remain with the caller under backpressure', () => {
  const p = peer();
  const socket = createBrowserSocket(p.socket as BrowserSocketPeer);
  p.open();
  p.socket.bufferedAmount = 1024 * 1024;
  expect(socket.send(new Uint8Array([1, 2]), false)).toBe(0);
  expect(socket.send(new Uint8Array([4]), true)).toBe(2);
  expect(p.sent).toEqual([]);
  expect(socket.flush()).toBe(1);
  p.socket.bufferedAmount = 0;
  expect(socket.send(new Uint8Array([1, 2]), false)).toBe(1);
  expect(p.sent).toEqual([new Uint8Array([1, 2])]);
  expect(socket.flush()).toBe(0);
  socket.dispose();
  expect(socket.send(new Uint8Array([1]), false)).toBe(3);
});

test.each(['text', new Uint8Array(65536).buffer])(
  'rejects malformed messages',
  (data) => {
    const p = peer();
    const socket = createBrowserSocket(p.socket as BrowserSocketPeer);
    p.open();
    p.message(data);
    expect(socket.receive()).toBe(-1);
    expect(p.socket.readyState).toBe(3);
  }
);

test('closes bounded receive queues instead of dropping required records', () => {
  const p = peer();
  const socket = createBrowserSocket(p.socket as BrowserSocketPeer);
  p.open();
  for (let i = 0; i < 257; i++) p.message(new Uint8Array([i % 256]).buffer);
  expect(socket.receive()).toBe(-1);
  expect(p.socket.readyState).toBe(3);
});
