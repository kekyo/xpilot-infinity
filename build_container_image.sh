#!/bin/bash
set -euo pipefail

project_root=${BUILD_CONTAINER_PROJECT_ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}
container_engine=${CONTAINER_ENGINE:-podman}
node_image=docker.io/library/node:24.19.0-trixie-slim@sha256:ab3eebe934147fee049b5eb83c570f68c849a13c930bdfa482de99fcdfa3b3de
source_url=https://github.com/kekyo/xpilot-infinity
version=${XPILOT_VERSION:-}
revision=${XPILOT_COMMIT_ID:-}
image_name=
platform=
skip_app_build=false
print_version=false

fail() { echo "build_container_image.sh: $*" >&2; exit 1; }
usage() {
    cat <<'USAGE'
Usage: ./build_container_image.sh [OPTIONS]

Build and verify the Node.js/Wasm server and bundled browser with Podman.
Options:
  --version VERSION   Product version (default: screw-up-derived)
  --revision COMMIT   Full source commit (default: screw-up-derived)
  --tag IMAGE         Local image name (default: localhost/xpilot-infinity-server:<version>)
  --platform PLATFORM Target platform (default: native)
  --node-image IMAGE  Node.js base image override
  --source-url URL    OCI source label
  --skip-app-build    Reuse matching server/browser build identities
  --print-version     Print the product version and exit
  --help              Show this help

The build verifies the image before tagging it. No image is pushed.
USAGE
}
while test $# -gt 0; do
    case $1 in
        --version|--revision|--tag|--platform|--node-image|--source-url)
            test $# -ge 2 && test -n "$2" || fail "$1 requires a value"
            case $1 in
                --version) version=$2;; --revision) revision=$2;;
                --tag) image_name=$2;; --platform) platform=$2;;
                --node-image) node_image=$2;; --source-url) source_url=$2;;
            esac
            shift 2;;
        --skip-app-build) skip_app_build=true; shift;;
        --print-version) print_version=true; shift;;
        --help|-h) usage; exit 0;;
        *) fail "unknown option: $1";;
    esac
done

command -v node >/dev/null || fail 'Install Node.js 24.19.0+ (24.x) or 26.4.0+.'
node -e 'if (!require("node:net").BoundSocket || !require("node:dgram").Socket.prototype.connectSync) { console.error("Install Node.js 24.19.0+ (24.x) or 26.4.0+."); process.exit(1); }'
if ! $skip_app_build && ! $print_version; then
    npm --prefix "$project_root/xpilot-browser" ci
    npm --prefix "$project_root/xpilot-server" ci
fi
export SCREW_UP=${SCREW_UP:-$project_root/xpilot-server/node_modules/.bin/screw-up}
metadata_resolver=${XPILOT_BUILD_METADATA_RESOLVER:-$project_root/config/resolve-build-metadata.sh}
metadata=$(XPILOT_VERSION=$version XPILOT_COMMIT_ID=$revision "$metadata_resolver")
{ IFS= read -r version; IFS= read -r revision; } <<< "$metadata"
if $print_version; then printf '%s\n' "$version"; exit 0; fi
export XPILOT_VERSION=$version XPILOT_COMMIT_ID=$revision
if test -z "$platform"; then
    case $(uname -m) in
        x86_64|amd64) platform=linux/amd64;;
        aarch64|arm64) platform=linux/arm64;;
        *) fail 'Only linux/amd64 and linux/arm64 are supported';;
    esac
fi
case $platform in linux/amd64|linux/arm64) ;; *) fail "unsupported platform: $platform";; esac
image_name=${image_name:-localhost/xpilot-infinity-server:$version}
command -v "$container_engine" >/dev/null || fail 'Install Podman.'
base_id=$("$container_engine" pull --quiet --platform "$platform" "$node_image")
"$container_engine" run --rm --platform "$platform" --entrypoint node "$base_id" -e \
    'if (!require("node:net").BoundSocket || !require("node:dgram").Socket.prototype.connectSync) process.exit(1)' \
    || fail "Cannot run supported Node.js on $platform; check the base image and QEMU/binfmt installation."
if ! $skip_app_build; then npm --prefix "$project_root/xpilot-server" run build; fi
node --input-type=module - "$project_root" "$version" "$revision" <<'JS'
import { readFile } from 'node:fs/promises';
const [root, version, commit] = process.argv.slice(2);
for (const path of ['xpilot-server/dist/build-info.json', 'xpilot-server/dist/web/build-info.json', 'xpilot-browser/dist/build-info.json']) {
    const actual = JSON.parse(await readFile(`${root}/${path}`, 'utf8'));
    if (actual.version !== version || actual.commit !== commit)
        throw new Error(`Build identity mismatch: ${path}; rebuild without --skip-app-build.`);
}
JS
context=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-container-build.XXXXXX")
candidate=localhost/xpilot-container-candidate:$$
cleanup() {
    "$container_engine" image rm "$candidate" >/dev/null 2>&1 || true
    rm -rf -- "$context"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$context/xpilot-server"
cp "$project_root/Dockerfile" "$project_root/.dockerignore" "$project_root/COPYING" "$context/"
cp -R "$project_root/xpilot-server/dist" "$context/xpilot-server/"
"$container_engine" build --pull=never --platform "$platform" --tag "$candidate" \
    --build-arg "NODE_IMAGE=$base_id" --build-arg "XPILOT_BASE_IMAGE=$node_image" \
    --build-arg "XPILOT_VERSION=$version" --build-arg "XPILOT_REVISION=$revision" \
    --build-arg "XPILOT_SOURCE=$source_url" "$context"
(
    cd "$project_root/xpilot-server"
    XPILOT_CONTAINER_IMAGE=$candidate XPILOT_CONTAINER_PLATFORM=$platform \
        node node_modules/vitest/vitest.mjs run tests/container.test.ts
)
"$container_engine" tag "$candidate" "$image_name"
echo "Built and verified local image: $image_name ($platform)"
