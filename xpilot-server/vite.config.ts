import { defineConfig } from 'vite';
import prettierMax from 'prettier-max';
import screwUp from 'screw-up';
import emsdkEnv from 'emsdk-env/vite';
import { fileURLToPath } from 'node:url';

export default defineConfig({
  plugins: [
    prettierMax(),
    emsdkEnv({
      emsdk: { targetVersion: '6.0.12' },
      generatedLoader: { enable: false },
      outDir: 'public/runtime',
      targets: {
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
  ],
  build: {
    target: 'node26',
    minify: false,
    lib: {
      entry: fileURLToPath(new URL('./src/runtime.ts', import.meta.url)),
      formats: ['es'],
      fileName: () => 'runtime.mjs',
    },
    rolldownOptions: { external: [/^node:/] },
  },
});
