import * as net from 'node:net';
import * as dgram from 'node:dgram';

/**
 * Require the public socket primitives used by the WASM server.
 * @throws Error when Node.js is too old to avoid private socket bindings.
 */
export const assertNodeRuntime = (): void => {
  if (
    typeof net.BoundSocket !== 'function' ||
    typeof dgram.Socket.prototype.bindSync !== 'function' ||
    typeof dgram.Socket.prototype.connectSync !== 'function'
  ) {
    throw new Error('XPilot Infinity requires Node.js 26.4 or later.');
  }
};
