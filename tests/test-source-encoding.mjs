import assert from 'node:assert/strict';
import { readdir, readFile } from 'node:fs/promises';
import { extname, join, relative, resolve } from 'node:path';

const sourceRootArgument = process.argv[2];
assert.ok(sourceRootArgument, 'source root path is required');

const sourceRoot = resolve(sourceRootArgument);
const ignoredRootDirectories = new Set([
  '.build',
  '.git',
  'artifacts',
  'autom4te.cache',
  'build',
  'vendor',
]);
// Package-manager and generated browser trees are not project source.
const ignoredBrowserDirectories = new Set([
  'xpilot-browser/node_modules', 'xpilot-browser/build', 'xpilot-browser/dist',
  'xpilot-browser/public', 'xpilot-browser/test-results', 'xpilot-browser/artifacts',
]);
const binaryExtensions = new Set([
  '.bmp',
  '.gif',
  '.icns',
  '.ico',
  '.png',
  '.pmsm',
  '.pmsp',
  '.ppm',
  '.ttf',
  '.wav',
  '.xpd',
]);
const nonUtf8TextPaths = new Set([
  'contrib/macosx/English.lproj/InfoPlist.strings',
]);
const decoder = new TextDecoder('utf-8', { fatal: true });
const failures = [];

const inspectFile = async (path) => {
  const relativePath = relative(sourceRoot, path);
  if (
    binaryExtensions.has(extname(path).toLowerCase()) ||
    nonUtf8TextPaths.has(relativePath)
  ) {
    return;
  }

  const contents = await readFile(path);
  if (contents.includes(0)) {
    return;
  }

  try {
    const decoded = decoder.decode(contents);
    if (decoded.includes('\uFFFD')) {
      failures.push(
        `${relativePath}: contains a Unicode replacement character`
      );
    }
  } catch {
    failures.push(`${relativePath}: is not valid UTF-8`);
  }
};

const inspectDirectory = async (directory) => {
  const entries = await readdir(directory, { withFileTypes: true });
  entries.sort((left, right) => left.name.localeCompare(right.name));

  for (const entry of entries) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      if (ignoredBrowserDirectories.has(relative(sourceRoot, path))) continue;
      if (
        directory !== sourceRoot ||
        !ignoredRootDirectories.has(entry.name)
      ) {
        await inspectDirectory(path);
      }
    } else if (entry.isFile()) {
      await inspectFile(path);
    }
  }
};

await inspectDirectory(sourceRoot);
assert.equal(failures.length, 0, failures.join('\n'));
process.stdout.write('Project source and text files use valid UTF-8\n');
