// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

/** Value and DOM registrations owned by one initialized native runtime. */
export interface InitializedRuntime<T> {
  /** Synchronous initialization result. */
  value: T;
  /** Remove registrations, including ones the native library left behind. */
  dispose: () => void;
}

/**
 * Own listeners installed synchronously by SDL on existing page targets.
 * @typeParam T Initialization result type.
 * @param targets Window, document and canvas borrowed from the page.
 * @param initialize Synchronous native initialization; must not return a Promise.
 * @returns Result and idempotent cleanup; failed initialization cleans itself up.
 * @remarks SDL's Emscripten mouse backend registers document callbacks outside
 * Emscripten's event registry. Their closures otherwise retain retired WASM
 * instances. Registration methods are restored before yielding to the browser;
 * no vendor internals or permanent page-wide patches are used.
 */
export const initializeWithListeners = <T>(
  targets: readonly EventTarget[],
  initialize: () => T
): InitializedRuntime<T> => {
  const listeners: Array<{
    target: EventTarget;
    type: string;
    listener: EventListenerOrEventListenerObject;
    capture: boolean;
  }> = [];
  const restorers: Array<() => void> = [];
  const dispose = () => {
    for (const { target, type, listener, capture } of listeners)
      target.removeEventListener(type, listener, capture);
    listeners.length = 0;
  };
  try {
    for (const target of targets) {
      const original = target.addEventListener;
      const descriptor = Object.getOwnPropertyDescriptor(
        target,
        'addEventListener'
      );
      target.addEventListener = (type, listener, options) => {
        original.call(target, type, listener, options);
        if (listener)
          listeners.push({
            target,
            type,
            listener,
            capture:
              typeof options === 'boolean' ? options : !!options?.capture,
          });
      };
      restorers.push(() => {
        if (descriptor)
          Object.defineProperty(target, 'addEventListener', descriptor);
        else Reflect.deleteProperty(target, 'addEventListener');
      });
    }
    return { value: initialize(), dispose };
  } catch (error) {
    dispose();
    throw error;
  } finally {
    for (const restore of restorers) restore();
  }
};
