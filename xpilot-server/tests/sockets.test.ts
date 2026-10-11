import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createSocket } from 'node:dgram';
import { createConnection } from 'node:net';
import { once } from 'node:events';
import { assertNodeRuntime } from '../src/runtime';

interface Probe {
  _probe_init: () => void;
  _probe_open: (tcp: number, port: number) => number;
  _probe_port: (fd: number) => number;
  _probe_accept: (fd: number) => number;
  _probe_echo: (fd: number, tcp: number) => number;
  _probe_close: (fd: number) => void;
}

describe('Node.js public socket backend', () => {
  let probe: Probe;
  const descriptors = new Set<number>();
  const pending = new Set<() => void>();
  const nextResult = (operation: () => number): Promise<number> =>
    new Promise((resolve) => {
      const check = () => {
        const result = operation();
        if (result !== 0) {
          pending.delete(check);
          resolve(result);
        }
      };
      pending.add(check);
      check();
    });
  const open = (tcp: number, port: number): number => {
    const fd = probe._probe_open(tcp, port);
    if (fd >= 0) descriptors.add(fd);
    return fd;
  };

  beforeAll(async () => {
    assertNodeRuntime();
    const url = new URL(
      '../build/test-runtime/socket-probe.mjs',
      import.meta.url
    );
    const { default: create } = await import(/* @vite-ignore */ url.href);
    probe = await create({
      onSocketReady: () => {
        for (const check of [...pending]) check();
      },
    });
    probe._probe_init();
  });

  afterAll(() => {
    pending.clear();
    for (const fd of descriptors) probe._probe_close(fd);
  });

  it('binds an ephemeral UDP port synchronously and exchanges a datagram', async () => {
    const fd = open(0, 0);
    expect(fd).toBeGreaterThan(0);
    const port = probe._probe_port(fd);
    expect(port).toBeGreaterThan(0);
    const peer = createSocket('udp4');
    try {
      const response = once(peer, 'message');
      const echoed = nextResult(() => probe._probe_echo(fd, 0));
      peer.send('UDP from Node', port, '127.0.0.1');
      expect(await echoed).toBe(13);
      const [data, address] = await response;
      expect(data.toString()).toBe('UDP from Node');
      expect(address.port).toBe(port);
    } finally {
      peer.close();
    }
  });

  it('accepts TCP and reports data and peer closure through readiness callbacks', async () => {
    const listener = open(1, 0);
    expect(listener).toBeGreaterThan(0);
    const port = probe._probe_port(listener);
    const accepted = nextResult(() => probe._probe_accept(listener));
    const peer = createConnection({ host: '127.0.0.1', port });
    try {
      await once(peer, 'connect');
      const fd = await accepted;
      expect(fd).toBeGreaterThan(0);
      descriptors.add(fd);
      const response = once(peer, 'data');
      const echoed = nextResult(() => probe._probe_echo(fd, 1));
      peer.write('TCP from Node');
      expect(await echoed).toBe(13);
      expect((await response)[0].toString()).toBe('TCP from Node');
      const closed = nextResult(() => probe._probe_echo(fd, 1));
      peer.end();
      expect(await closed).toBe(-1);
    } finally {
      peer.destroy();
    }
  });

  it('reports a TCP bind conflict before publishing a listener port', () => {
    const listener = open(1, 0);
    expect(listener).toBeGreaterThan(0);
    expect(open(1, probe._probe_port(listener))).toBeLessThan(0);
  });
});
