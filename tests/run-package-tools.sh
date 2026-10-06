#!/bin/sh

set -eu

node -e 'process.exit(Number(process.versions.node.split(".")[0]) >= 16 ? 0 : 1)' \
    || { echo 'Package template tools require Node.js 16 or newer' >&2; exit 1; }

# npm must load its own dependencies and launch the selected Node interpreter.
# A standalone Node paired with the distro npm can fail before running a CLI.
expected_node=$(node -p 'process.execPath')
actual_node=$(npm exec --offline --call 'node -p "process.execPath"')
test "$actual_node" = "$expected_node" \
    || { echo 'npm exec selected a different Node interpreter' >&2; exit 1; }

echo 'Package Node.js and npm execution passed'
