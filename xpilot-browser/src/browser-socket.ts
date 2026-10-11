// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** Browser WebSocket operations consumed by the record adapter. */
export interface BrowserSocketPeer extends EventTarget {
  /** WebSocket connection state. */
  readonly readyState: number;
  /** Bytes accepted by the browser but not yet transmitted. */
  readonly bufferedAmount: number;
  /** Negotiated XPilot protocol. */
  readonly protocol: string;
  /** Incoming binary representation. */
  binaryType: BinaryType;
  /** Copy a complete binary message to the browser's send queue. */
  send: (payload: Uint8Array<ArrayBuffer>) => void;
  /** Begin an orderly close after already submitted messages. */
  close: () => void;
}

/** Bounded, nonblocking logical-record adapter for one browser socket. */
export interface BrowserSocket {
  /** One record, undefined while pending, or -1 when closed. */
  receive: () => Uint8Array | undefined | -1;
  /** Result matching record_send_result_t; required backpressure retains caller ownership. */
  send: (record: Uint8Array, transient: boolean) => number;
  /** Result matching record_flush_result_t; pending bytes are not acknowledged delivery. */
  flush: () => number;
  /** Current connection state, including protocol failures. */
  state: () => 'connecting' | 'open' | 'closed';
  /** Detach callbacks, clear owned storage and close the socket. */
  dispose: () => void;
}

/**
 * Derive the fixed game endpoint from the page's origin.
 * @param pageURL Absolute HTTP or HTTPS page URL.
 * @returns Same-origin ws/wss URL without page query, credentials or fragment.
 */
export const sameOriginEndpoint = (pageURL: string): string => {
  const page = new URL(pageURL);
  if (page.protocol !== 'http:' && page.protocol !== 'https:')
    throw new Error('XPilot requires an HTTP or HTTPS page');
  const endpoint = new URL('/xpilot', page.origin);
  endpoint.protocol = page.protocol === 'https:' ? 'wss:' : 'ws:';
  return endpoint.href;
};

/**
 * Adapt a newly created XPilot WebSocket without waiting for it to open.
 * @param peer Socket owned by the returned adapter.
 * @param onActivity Called after connection or incoming-record progress.
 * @returns Record operations with a 1 MiB / 256 record incoming limit.
 */
export const createBrowserSocket = (
  peer: BrowserSocketPeer,
  onActivity: () => void = () => {}
): BrowserSocket => {
  const maximumBytes = 1024 * 1024;
  const queue: Uint8Array[] = [];
  let bytes = 0;
  let closed = false;
  peer.binaryType = 'arraybuffer';
  const fail = () => {
    closed = true;
    queue.length = 0;
    bytes = 0;
    peer.close();
  };
  const onOpen = () => {
    if (peer.protocol !== 'xpilot-infinity-v1') fail();
    onActivity();
  };
  const onClose = () => {
    closed = true;
    onActivity();
  };
  const onMessage = (event: Event) => {
    if (closed) return;
    const data: unknown = (event as MessageEvent).data;
    if (
      !(data instanceof ArrayBuffer) ||
      data.byteLength === 0 ||
      data.byteLength > 65535 ||
      queue.length >= 256 ||
      bytes + data.byteLength > maximumBytes
    ) {
      fail();
      return;
    }
    queue.push(new Uint8Array(data));
    bytes += data.byteLength;
    onActivity();
  };
  peer.addEventListener('open', onOpen);
  peer.addEventListener('message', onMessage);
  peer.addEventListener('close', onClose);
  peer.addEventListener('error', fail);
  const state = () =>
    closed || peer.readyState >= 2
      ? ('closed' as const)
      : peer.readyState === 1
        ? ('open' as const)
        : ('connecting' as const);
  return {
    state,
    receive: () => {
      const record = queue.shift();
      if (record !== undefined) {
        bytes -= record.byteLength;
        return record;
      }
      return state() === 'closed' ? -1 : undefined;
    },
    send: (record, transient) => {
      if (state() === 'closed') return 3;
      if (record.byteLength === 0 || record.byteLength > 65535) {
        fail();
        return -1;
      }
      if (
        state() !== 'open' ||
        peer.bufferedAmount + record.byteLength > maximumBytes
      )
        return transient ? 2 : 0;
      try {
        peer.send(record.slice());
        return 1;
      } catch {
        fail();
        return -1;
      }
    },
    flush: () =>
      state() === 'closed'
        ? 2
        : peer.bufferedAmount > 0 || state() === 'connecting'
          ? 1
          : 0,
    dispose: () => {
      peer.removeEventListener('open', onOpen);
      peer.removeEventListener('message', onMessage);
      peer.removeEventListener('close', onClose);
      peer.removeEventListener('error', fail);
      fail();
    },
  };
};
