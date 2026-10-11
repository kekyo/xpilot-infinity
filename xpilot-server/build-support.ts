import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { availableParallelism } from 'node:os';
import { resolve } from 'node:path';
import { prepareEmsdk } from 'emsdk-env';
import { buildCandidateVariables, runScriptOnceToText } from 'funcity';

/** Fixed SDK used by every server WASM object and archive. */
export const sdkVersion = '6.0.12';

/**
 * Prepare the SDK-managed dependency archives and native build identity.
 * @param root Absolute npm source package directory.
 * @returns Dependency prefix and generated C header directory.
 */
export const prepareServerBuild = async (
  root: string
): Promise<{ prefix: string; generated: string }> => {
  const sdk = await prepareEmsdk({ targetVersion: sdkVersion });
  const build = resolve(root, 'build/wasm', sdkVersion);
  const prefix = resolve(build, 'prefix');
  const exec = promisify(execFile);
  const dependencies = await exec(
    'make',
    [
      '-C',
      resolve(root, 'wasm'),
      'dependencies',
      `EMSDK_ROOT=${sdk}`,
      `BUILD_DIR=${build}`,
      `PREFIX=${prefix}`,
      `JOBS=${Math.min(8, availableParallelism())}`,
    ],
    { maxBuffer: 8 * 1024 * 1024 }
  );
  process.stdout.write(dependencies.stdout);
  process.stderr.write(dependencies.stderr);
  const { stdout } = await exec(
    resolve(root, '../config/resolve-build-metadata.sh'),
    ['--force'],
    {
      env: {
        ...process.env,
        SCREW_UP: resolve(root, 'node_modules/.bin/screw-up'),
      },
    }
  );
  const [version, commit] = stdout.trim().split('\n');
  const template = await readFile(
    resolve(root, '../xpilot-browser/wasm/version.h.fc'),
    'utf8'
  );
  const header = await runScriptOnceToText(template, {
    variables: buildCandidateVariables({ version, commit }),
    logs: [],
    sourceId: 'version.h.fc',
  });
  if (header === undefined)
    throw new Error('Could not generate server build identity');
  const generated = resolve(root, 'build/generated');
  await mkdir(generated, { recursive: true });
  await mkdir(resolve(root, 'build/test-runtime'), { recursive: true });
  await writeFile(resolve(generated, 'version.h'), header);
  await rm(resolve(root, 'public/runtime'), { recursive: true, force: true });
  const assets = resolve(root, 'public/assets');
  await rm(assets, { recursive: true, force: true });
  await mkdir(assets, { recursive: true });
  await cp(resolve(root, '../lib'), assets, {
    recursive: true,
    filter: (source) => !/(^|\/)Makefile(?:\.am|\.in)?$/.test(source),
  });
  await rm(resolve(root, 'public/web'), { recursive: true, force: true });
  await cp(
    resolve(root, '../xpilot-browser/dist'),
    resolve(root, 'public/web'),
    { recursive: true }
  );
  await cp(resolve(root, '../COPYING'), resolve(root, 'COPYING'));
  const info = await runScriptOnceToText(
    '{"version":"{{version}}","commit":"{{commit}}"}\n',
    {
      variables: buildCandidateVariables({ version, commit }),
      logs: [],
      sourceId: 'build-info.json',
    }
  );
  if (info === undefined)
    throw new Error('Could not generate server build metadata');
  await writeFile(resolve(root, 'public/build-info.json'), info);
  const licenses = resolve(root, 'public/licenses');
  await mkdir(licenses, { recursive: true });
  await cp(
    resolve(sdk, 'upstream/emscripten/LICENSE'),
    resolve(licenses, 'Emscripten.txt')
  );
  await cp(
    resolve(sdk, 'upstream/emscripten/system/lib/libc/musl/COPYRIGHT'),
    resolve(licenses, 'musl.txt')
  );
  for (const [source, name] of [
    ['vendor/expat/expat/COPYING', 'Expat.txt'],
    ['vendor/wslay/COPYING', 'wslay.txt'],
    ['vendor/zlib/README', 'zlib.txt'],
    ['vendor/mtwister/README.md', 'mtwister.md'],
  ] as const)
    await cp(resolve(root, '..', source), resolve(licenses, name));
  return { prefix, generated };
};
