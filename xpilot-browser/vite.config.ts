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
import { prepareBrowserDependencies, sdkVersion } from './build-support.ts';
import { prepareBrowserMetadata } from './build-metadata.ts';
import { gameSources } from './game-sources.ts';

export default defineConfig(async () => {
  const root = fileURLToPath(new URL('.', import.meta.url));
  const prefix = await prepareBrowserDependencies(root);
  const generated = await prepareBrowserMetadata(root);
  return {
    plugins: [
      prettierMax(),
      emsdkEnv({
        emsdk: { targetVersion: sdkVersion },
        generatedLoader: { enable: false },
        outDir: 'public/runtime',
        targets: {
          'xpilot-game': {
            outFile: 'xpilot-game.mjs',
            sources: gameSources,
            options: [
              '-DSOUND=1',
              '-O2',
              '-std=c99',
              '-include',
              resolve(root, 'wasm/platform.h'),
            ],
            includeDirs: [
              generated,
              resolve(root, 'wasm'),
              resolve(root, '../src/common'),
              resolve(root, '../src/client'),
              resolve(root, '../src/client/sdl'),
              resolve(root, '../vendor/libtess2/Include'),
              resolve(root, '../vendor/mtwister'),
              resolve(prefix, 'include'),
            ],
            linkOptions: [
              '-O2',
              '--no-entry',
              resolve(prefix, 'lib/libSDL3_ttf.a'),
              resolve(prefix, 'lib/libSDL3_image.a'),
              resolve(prefix, 'lib/libSDL3.a'),
              resolve(prefix, 'lib/libz.a'),
              '-lm',
              '--preload-file',
              resolve(root, 'build/assets') + '@/assets',
            ],
            linkDirectives: {
              MODULARIZE: 1,
              EXPORT_ES6: 1,
              ENVIRONMENT: 'web',
              MIN_WEBGL_VERSION: 2,
              MAX_WEBGL_VERSION: 2,
              ALLOW_MEMORY_GROWTH: 1,
              EXPORTED_RUNTIME_METHODS: ['HEAPU8', 'UTF8ToString', 'FS'],
            },
            exports: [
              '_malloc',
              '_free',
              '_xp_browser_session_begin',
              '_xp_browser_session_step',
              '_xp_browser_session_error',
              '_xp_browser_session_close',
              '_xp_game_resume_begin',
              '_xp_game_resume_step',
              '_xp_game_quit_step',
              '_xp_game_clear_input',
              '_xp_game_init',
              '_xp_game_begin',
              '_xp_game_step',
              '_xp_game_stop',
              '_xp_game_resize',
              '_xp_game_chat',
              '_xp_game_frame',
              '_xp_game_position',
              '_xp_game_held',
              '_xp_game_width',
              '_xp_game_save_settings',
              '_xp_game_context_lost',
              '_xp_game_context_restored',
              '_xp_game_text_scale_changed',
              '_xp_game_pointer',
              '_xp_game_fullscreen',
              '_xp_game_heading',
              '_xp_game_shots',
              '_xp_game_message',
              '_xp_game_map_url',
              '_xp_game_assets_ready',
            ],
          },
          'xpilot-browser': {
            outFile: 'xpilot-browser.mjs',
            sources: [
              'driver.c',
              'browser_session.c',
              '../../src/common/record_transport.c',
              '../../src/common/record_session.c',
              '../../src/common/session_protocol.c',
              '../../src/common/game_transport.c',
              '../../src/common/strlcpy.c',
              '../../src/client/client_session.c',
              '../../src/client/sdl/renderer.c',
              '../../src/client/sdl/renderer_gl_core.c',
            ],
            options: [
              '-O2',
              '-std=c99',
              '-Wall',
              '-Wextra',
              '-Werror',
              '-include',
              resolve(root, 'wasm/platform.h'),
            ],
            includeDirs: [
              generated,
              resolve(root, '../src/common'),
              resolve(root, '../src/client'),
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
              EXPORTED_RUNTIME_METHODS: ['HEAPU8', 'UTF8ToString'],
            },
            exports: [
              '_xp_browser_session_begin',
              '_xp_browser_session_step',
              '_xp_browser_session_error',
              '_xp_browser_session_close',
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
    server: {
      proxy: {
        '/xpilot': {
          target: process.env.XPILOT_SERVER_URL ?? 'ws://127.0.0.1:15345',
          ws: true,
        },
      },
    },
    build: {
      rolldownOptions: {
        // These plugins deliberately compile WASM and run formatting/type checks.
        checks: { bundlerTimings: false },
        external: [],
        input: {
          game: resolve(root, 'index.html'),
          preview: resolve(root, 'preview.html'),
        },
      },
      target: 'es2022',
      sourcemap: true,
      minify: false,
    },
  };
});
