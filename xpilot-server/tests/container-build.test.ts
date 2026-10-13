import { expect, it } from 'vitest';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

it.each([
  'local',
  'publish',
  'build failure',
  'verification failure',
  'stale assets',
  'invalid platform',
])(
  'builds and publishes only matching verified platform images: %s',
  async (scenario) => {
    const source = fileURLToPath(new URL('../..', import.meta.url));
    const root = await mkdtemp(resolve(tmpdir(), 'xpilot-container-script-'));
    const log = resolve(root, 'calls.jsonl');
    const commit = '0123456701234567012345670123456701234567';
    try {
      for (const path of [
        'config',
        'xpilot-server/dist/web',
        'xpilot-browser/dist',
        'xpilot-server/node_modules/vitest',
      ])
        await mkdir(resolve(root, path), { recursive: true });
      for (const path of [
        'build_container_image.sh',
        'Dockerfile',
        '.dockerignore',
        'COPYING',
        'config/resolve-build-metadata.sh',
      ])
        await cp(resolve(source, path), resolve(root, path));
      for (const path of [
        'xpilot-server/dist/build-info.json',
        'xpilot-server/dist/web/build-info.json',
        'xpilot-browser/dist/build-info.json',
      ])
        await writeFile(
          resolve(root, path),
          JSON.stringify({
            version: scenario === 'stale assets' ? '0.0.0' : '9.8.7',
            commit,
          })
        );
      await writeFile(log, '');
      const engine = resolve(root, 'podman');
      await writeFile(
        engine,
        `#!/usr/bin/env node
const fs = require('node:fs');
const args = process.argv.slice(2);
fs.appendFileSync(process.env.TEST_LOG, JSON.stringify(args)+'\\n');
if (args[0]==='pull') console.log('base-'+args[args.indexOf('--platform')+1]);
if (args[0]==='build' && process.env.TEST_SCENARIO==='build failure' && args.includes('linux/arm64')) process.exit(1);
if (args[0]==='manifest' && args[1]==='inspect') console.log(JSON.stringify({manifests:[{platform:{os:'linux',architecture:'amd64'}},{platform:{os:'linux',architecture:'arm64'}}]}));
`,
        { mode: 0o755 }
      );
      await writeFile(
        resolve(root, 'xpilot-server/node_modules/vitest/vitest.mjs'),
        `import {appendFileSync} from 'node:fs';
appendFileSync(process.env.TEST_LOG, JSON.stringify(['verify',process.env.XPILOT_CONTAINER_PLATFORM,process.env.XPILOT_CONTAINER_IMAGE])+'\\n');
if (process.env.TEST_SCENARIO==='verification failure' && process.env.XPILOT_CONTAINER_PLATFORM==='linux/arm64') process.exit(1);
`
      );
      let failed = false;
      try {
        await promisify(execFile)(
          resolve(root, 'build_container_image.sh'),
          [
            '--skip-app-build',
            '--version',
            '9.8.7',
            '--revision',
            commit,
            '--tag',
            'localhost/xpilot-build-test:9.8.7',
            '--jobs',
            '2',
            ...(scenario === 'local' ? [] : ['--push']),
            ...(scenario === 'invalid platform'
              ? ['--platform', 'linux/amd64,']
              : []),
          ],
          {
            env: {
              ...process.env,
              BUILD_CONTAINER_PROJECT_ROOT: root,
              CONTAINER_ENGINE: engine,
              TEST_LOG: log,
              TEST_SCENARIO: scenario,
            },
          }
        );
      } catch {
        failed = true;
      }
      const calls: string[][] = (await readFile(log, 'utf8'))
        .trim()
        .split('\n')
        .filter(Boolean)
        .map((line) => JSON.parse(line));
      if (scenario === 'local' || scenario === 'publish') {
        expect(failed).toBe(false);
        // Removing a manifest object also removes every alias attached to it.
        expect(
          calls.some((args) => args[0] === 'manifest' && args[1] === 'rm')
        ).toBe(false);
        const builds = calls.filter((args) => args[0] === 'build');
        expect(builds).toHaveLength(2);
        for (const platform of ['linux/amd64', 'linux/arm64']) {
          const build = builds.find((args) => args.includes(platform))!;
          expect(build).toContain(`NODE_IMAGE=base-${platform}`);
          expect(
            calls.some((args) => args[0] === 'verify' && args[1] === platform)
          ).toBe(true);
        }
        expect(
          calls.filter((args) => args[0] === 'manifest' && args[1] === 'add')
        ).toHaveLength(2);
        const pushes = calls.filter(
          (args) => args[0] === 'manifest' && args[1] === 'push'
        );
        expect(pushes).toHaveLength(scenario === 'publish' ? 2 : 0);
        if (scenario === 'publish') {
          expect(pushes[0]).toContain('--all');
          expect(pushes[0]).toContain(
            'docker://docker.io/kekyo/xpilot-infinity-server:9.8.7'
          );
          expect(pushes[1]).toContain(
            'docker://docker.io/kekyo/xpilot-infinity-server:latest'
          );
          expect(calls.indexOf(pushes[0])).toBeGreaterThan(
            calls.indexOf(calls.filter((args) => args[0] === 'verify').at(-1)!)
          );
        }
      } else {
        expect(failed).toBe(true);
        if (scenario === 'build failure' || scenario === 'verification failure')
          expect(calls.filter((args) => args[0] === 'build')).toHaveLength(2);
        if (scenario === 'verification failure')
          expect(calls.filter((args) => args[0] === 'verify')).toHaveLength(2);
        expect(
          calls.some((args) => args[0] === 'tag' || args.includes('push'))
        ).toBe(false);
        if (scenario === 'stale assets' || scenario === 'invalid platform')
          expect(calls.some((args) => args[0] === 'build')).toBe(false);
      }
    } finally {
      await rm(root, { recursive: true, force: true });
    }
  },
  30000
);
