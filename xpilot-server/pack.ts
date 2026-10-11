import { cp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildCandidateVariables, runScriptOnceToText } from 'funcity';

const root = fileURLToPath(new URL('.', import.meta.url));
const info = JSON.parse(
  await readFile(resolve(root, 'dist/build-info.json'), 'utf8')
);
const browser = JSON.parse(
  await readFile(resolve(root, 'dist/web/build-info.json'), 'utf8')
);
if (info.version !== browser.version || info.commit !== browser.commit)
  throw new Error(
    'Server and browser build identities differ; rebuild both together.'
  );
const manifest = JSON.parse(
  await readFile(resolve(root, 'package.json'), 'utf8')
);
manifest.version = info.version;
manifest.gitHead = info.commit;
delete manifest.devDependencies;
delete manifest.scripts;
const text = await runScriptOnceToText('{{json}}\n', {
  variables: buildCandidateVariables({
    json: JSON.stringify(manifest, null, 2),
  }),
  logs: [],
  sourceId: 'package.json',
});
if (text === undefined) throw new Error('Could not generate release manifest');
const stage = resolve(root, 'build/package');
await rm(stage, { recursive: true, force: true });
await mkdir(stage, { recursive: true });
await writeFile(resolve(stage, 'package.json'), text);
for (const name of ['dist', 'COPYING', 'README.md'])
  await cp(resolve(root, name), resolve(stage, name), { recursive: true });
const destination = resolve(process.argv[2] ?? resolve(root, 'artifacts'));
await mkdir(destination, { recursive: true });
const result = await promisify(execFile)(
  process.execPath,
  [
    resolve(root, 'node_modules/.bin/screw-up'),
    'pack',
    stage,
    '--no-git-version-override',
    '--inheritable-fields',
    'version',
    '--no-merge-files',
    '--pack-destination',
    destination,
  ],
  { cwd: root }
);
process.stdout.write(result.stdout);
process.stderr.write(result.stderr);
