// xpilot-infinity - XPilot Infinity browser client.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under GPL-2.0-only
// https://github.com/kekyo/xpilot-infinity/

import { defineConfig } from 'vite';
import prettierMax from 'prettier-max';
import screwUp from 'screw-up';
import emsdkEnv from 'emsdk-env/vite';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import { prepareBrowserDependencies, sdkVersion } from './build-support';

export default defineConfig(async () => {
  const root = fileURLToPath(new URL('.', import.meta.url));
  const prefix = await prepareBrowserDependencies(root);
  return {
    plugins: [
      prettierMax(),
      emsdkEnv({
        emsdk: { targetVersion: sdkVersion },
        generatedLoader: { enable: false },
        outDir: 'public/runtime',
        targets: {
          'xpilot-browser': {
            outFile: 'xpilot-browser.mjs',
            sources: [
              'driver.c',
              '../../src/client/sdl/renderer.c',
              '../../src/client/sdl/renderer_gl_core.c',
            ],
            options: ['-O2', '-std=c99', '-Wall', '-Wextra', '-Werror'],
            includeDirs: [
              resolve(prefix, 'include'),
              resolve(root, '../src/client/sdl'),
            ],
            linkOptions: [
              '-O2',
              '--no-entry',
              resolve(prefix, 'lib/libSDL3.a'),
              '-lm',
            ],
            linkDirectives: {
              MODULARIZE: 1,
              EXPORT_ES6: 1,
              ENVIRONMENT: 'web',
              MIN_WEBGL_VERSION: 2,
              MAX_WEBGL_VERSION: 2,
              ALLOW_MEMORY_GROWTH: 1,
              EXPORTED_RUNTIME_METHODS: ['HEAPU8'],
            },
            exports: [
              '_malloc',
              '_free',
              '_xp_browser_init',
              '_xp_browser_frame',
              '_xp_browser_resize',
              '_xp_browser_upload_text',
              '_xp_browser_shutdown',
              '_xp_browser_scene_x',
              '_xp_browser_scene_y',
            ],
          },
        },
      }),
      screwUp({
        outputMetadataFile: true,
      }),
    ],
    build: {
      rolldownOptions: {
        external: [],
      },
      target: 'es2022',
      sourcemap: true,
      minify: false,
    },
  };
});
