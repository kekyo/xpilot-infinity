#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
node -e 'if (!require("node:net").BoundSocket || !require("node:dgram").Socket.prototype.bindSync) { console.error("Building the npm release requires Node.js 24.x (24.19.0+) or 26.4.0+."); process.exit(1); }'

npm --prefix "$project_root/xpilot-browser" ci
npm --prefix "$project_root/xpilot-server" ci
metadata=$("$project_root/config/resolve-build-metadata.sh")
XPILOT_VERSION=$(printf '%s\n' "$metadata" | sed -n '1p')
XPILOT_COMMIT_ID=$(printf '%s\n' "$metadata" | sed -n '2p')
export XPILOT_VERSION XPILOT_COMMIT_ID
npm --prefix "$project_root/xpilot-server" run build
npm --prefix "$project_root/xpilot-server" run pack:built -- "$project_root/artifacts/npm"
