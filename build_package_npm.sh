#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
node -e 'if (!require("node:net").BoundSocket || !require("node:dgram").Socket.prototype.bindSync) { console.error("Building the npm release requires Node.js 26.4 or later."); process.exit(1); }'

npm --prefix "$project_root/xpilot-browser" ci
npm --prefix "$project_root/xpilot-server" ci
SCREW_UP="$project_root/xpilot-server/node_modules/.bin/screw-up"
export SCREW_UP
metadata=$("$project_root/config/resolve-build-metadata.sh" --force)
XPILOT_VERSION=$(printf '%s\n' "$metadata" | sed -n '1p')
XPILOT_COMMIT_ID=$(printf '%s\n' "$metadata" | sed -n '2p')
export XPILOT_VERSION XPILOT_COMMIT_ID
npm --prefix "$project_root/xpilot-server" run build
npm --prefix "$project_root/xpilot-server" run pack:built -- "$project_root/artifacts/npm"
