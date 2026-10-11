import { defineConfig, type UserConfig } from 'vite';
import prettierMax from 'prettier-max';
import screwUp from 'screw-up';
import emsdkEnv from 'emsdk-env/vite';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import { chmod } from 'node:fs/promises';
import { prepareServerBuild, sdkVersion } from './build-support';
import { serverSources } from './server-sources';

export default defineConfig(async (): Promise<UserConfig> => {
  const root = fileURLToPath(new URL('.', import.meta.url));
  const { prefix, generated } = await prepareServerBuild(root);
  return {
    plugins: [
      prettierMax(),
      emsdkEnv({
        emsdk: { targetVersion: sdkVersion },
        generatedLoader: { enable: false },
        outDir: 'public/runtime',
        targets: {
          'xpilot-server': {
            outFile: 'xpilot-server.mjs',
            sources: serverSources,
            options: [
              '-std=c99',
              '-O2',
              '-sSUPPORT_LONGJMP=wasm',
              '-include',
              resolve(root, 'wasm/platform.h'),
            ],
            includeDirs: [
              generated,
              resolve(root, '../src/common'),
              resolve(root, '../src/server'),
              resolve(root, '../vendor/mtwister'),
              resolve(root, '../vendor/wslay/lib'),
              resolve(root, '../vendor/wslay/lib/includes'),
              resolve(prefix, 'include'),
            ],
            linkOptions: [
              '--no-entry',
              '-O2',
              '-Wl,--wrap=exit',
              '-lnodefs.js',
              resolve(prefix, 'lib/libexpat.a'),
              resolve(prefix, 'lib/libz.a'),
              '-lm',
            ],
            linkDirectives: {
              MODULARIZE: 1,
              EXPORT_ES6: 1,
              ENVIRONMENT: 'node',
              NODERAWSOCKETS: 1,
              SUPPORT_LONGJMP: 'wasm',
              ALLOW_MEMORY_GROWTH: 1,
              EXPORTED_RUNTIME_METHODS: ['FS', 'NODEFS', 'HEAPU8', 'HEAPU32'],
            },
            exports: [
              '_malloc',
              '_free',
              '_xp_server_start',
              '_xp_server_pump',
              '_xp_server_stop',
              '_xp_server_frames',
              '_xp_server_robots',
              '_xp_server_fps',
              '_xp_server_status',
            ],
          },
          'socket-probe': {
            outFile: 'socket-probe.mjs',
            sources: ['socket-probe.c'],
            options: ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror'],
            linkOptions: ['--no-entry', '-O2'],
            linkDirectives: {
              MODULARIZE: 1,
              EXPORT_ES6: 1,
              ENVIRONMENT: 'node',
              NODERAWSOCKETS: 1,
            },
            exports: [
              '_probe_init',
              '_probe_open',
              '_probe_port',
              '_probe_accept',
              '_probe_echo',
              '_probe_close',
            ],
          },
        },
      }),
      screwUp(),
      {
        name: 'server-cli-permissions',
        writeBundle: async () => {
          await chmod(resolve(root, 'dist/cli.mjs'), 0o755);
        },
      },
    ],
    build: {
      target: 'node26',
      minify: false,
      lib: {
        entry: fileURLToPath(new URL('./src/cli.ts', import.meta.url)),
        formats: ['es'],
        fileName: () => 'cli.mjs',
      },
      rolldownOptions: { external: [/^node:/] },
    },
  };
});
