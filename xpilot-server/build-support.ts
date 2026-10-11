import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
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
  await writeFile(resolve(generated, 'version.h'), header);
  return { prefix, generated };
};
