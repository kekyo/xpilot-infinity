// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['tests/**/*.test.ts'],
  },
});
