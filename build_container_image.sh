#!/bin/bash
set -euo pipefail

project_root=${BUILD_CONTAINER_PROJECT_ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}
container_engine=${CONTAINER_ENGINE:-podman}
node_image=docker.io/library/node:24.19.0-trixie-slim@sha256:ab3eebe934147fee049b5eb83c570f68c849a13c930bdfa482de99fcdfa3b3de
source_url=https://github.com/kekyo/xpilot-infinity
registry_image=docker.io/kekyo/xpilot-infinity
version=${XPILOT_VERSION:-}
revision=${XPILOT_COMMIT_ID:-}
image_name=
platforms=linux/amd64,linux/arm64
jobs=2
skip_app_build=false
print_version=false
push=false

fail() { echo "build_container_image.sh: $*" >&2; exit 1; }
usage() {
    cat <<'USAGE'
Usage: ./build_container_image.sh [OPTIONS]

Build and verify the Node.js/Wasm server and bundled browser with Podman.
Options:
  --version VERSION   Product version (default: screw-up-derived)
  --revision COMMIT   Full source commit (default: screw-up-derived)
  --tag IMAGE         Local image/manifest (default: localhost/xpilot-infinity:<version>)
  --platform LIST     Target platforms (default: linux/amd64,linux/arm64)
  --jobs NUMBER       Parallel image builds, 1 or 2 (default: 2)
  --node-image IMAGE  Override the pinned Node.js base image
  --source-url URL    OCI source label
  --skip-app-build    Reuse matching server/browser build identities
  --push              Publish verified version and latest tags
  --registry-image NAME  Repository to publish (default: docker.io/kekyo/xpilot-infinity)
  --print-version     Print the product version and exit
  --help              Show this help

The application is built once. Each CPU image is executed and tested before
updating the local tag or publishing. Foreign CPU execution needs QEMU/binfmt.
USAGE
}
while test $# -gt 0; do
    case $1 in
        --version|--revision|--tag|--platform|--node-image|--source-url|--jobs|--registry-image)
            test $# -ge 2 && test -n "$2" || fail "$1 requires a value"
            case $1 in
                --version) version=$2;; --revision) revision=$2;;
                --tag) image_name=$2;; --platform) platforms=$2;;
                --node-image) node_image=$2;; --source-url) source_url=$2;;
                --jobs) jobs=$2;; --registry-image) registry_image=$2;;
            esac
            shift 2;;
        --skip-app-build) skip_app_build=true; shift;;
        --push) push=true; shift;;
        --print-version) print_version=true; shift;;
        --help|-h) usage; exit 0;;
        *) fail "unknown option: $1";;
    esac
done
case $platforms in
    linux/amd64|linux/arm64|linux/amd64,linux/arm64|linux/arm64,linux/amd64) ;;
    *) fail "unsupported or repeated platforms: $platforms";;
esac
case $jobs in 1|2) ;; *) fail '--jobs must be 1 or 2';; esac
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
image_name=${image_name:-localhost/xpilot-infinity:$version}
command -v "$container_engine" >/dev/null || fail 'Install Podman.'
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
IFS=, read -ra targets <<< "$platforms"
declare -A base_images
# Pull sequentially and retain IDs: parallel FROM instructions must not race
# on Podman's single architecture tag in local storage.
for platform in "${targets[@]}"; do
    base_images[$platform]=$("$container_engine" pull --quiet --platform "$platform" "$node_image")
    "$container_engine" run --rm --platform "$platform" --entrypoint node "${base_images[$platform]}" -e \
        'if (!require("node:net").BoundSocket || !require("node:dgram").Socket.prototype.connectSync) process.exit(1)' \
        || fail "Cannot run supported Node.js on $platform; check the base image and QEMU/binfmt installation."
done
context=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-container-build.XXXXXX")
candidate=localhost/xpilot-container-candidate:$$-$RANDOM
images=()
running=()
retained=false
cleanup() {
    for pid in "${running[@]}"; do kill "$pid" 2>/dev/null || true; done
    for pid in "${running[@]}"; do wait "$pid" 2>/dev/null || true; done
    if $retained; then
        # The final tag aliases the same manifest/image object. Remove only the
        # temporary names; image/manifest rm would also invalidate that tag.
        if test "${#targets[@]}" -gt 1; then
            "$container_engine" untag "$candidate" "$candidate" >/dev/null 2>&1 || true
        fi
        for image in "${images[@]}"; do "$container_engine" untag "$image" "$image" >/dev/null 2>&1 || true; done
    else
        "$container_engine" manifest rm "$candidate" >/dev/null 2>&1 || true
        for image in "${images[@]}"; do "$container_engine" image rm "$image" >/dev/null 2>&1 || true; done
    fi
    rm -rf -- "$context"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$context/xpilot-server"
cp "$project_root/Dockerfile" "$project_root/.dockerignore" "$project_root/COPYING" "$context/"
cp -R "$project_root/xpilot-server/dist" "$context/xpilot-server/"
build_platform() {
    "$container_engine" build --pull=never --platform "$1" --tag "$2" \
        --build-arg "NODE_IMAGE=${base_images[$1]}" --build-arg "XPILOT_BASE_IMAGE=$node_image" \
        --build-arg "XPILOT_VERSION=$version" --build-arg "XPILOT_REVISION=$revision" \
        --build-arg "XPILOT_SOURCE=$source_url" "$context"
}
wait_builds() {
    local failed=false pid
    for pid in "${running[@]}"; do wait "$pid" || failed=true; done
    running=()
    ! $failed || fail 'A platform image build failed; no image was published.'
}
for platform in "${targets[@]}"; do
    image=$candidate-${platform#linux/}
    images+=("$image")
    build_platform "$platform" "$image" &
    running+=("$!")
    if test "${#running[@]}" -ge "$jobs"; then wait_builds; fi
done
wait_builds
for index in "${!targets[@]}"; do
    (
        cd "$project_root/xpilot-server"
        XPILOT_CONTAINER_IMAGE=${images[$index]} XPILOT_CONTAINER_PLATFORM=${targets[$index]} \
            node node_modules/vitest/vitest.mjs run tests/container.test.ts
    )
done
if test "${#targets[@]}" -gt 1; then
    "$container_engine" manifest create "$candidate"
    for image in "${images[@]}"; do "$container_engine" manifest add "$candidate" "containers-storage:$image"; done
    "$container_engine" manifest inspect "$candidate" | node -e '
const list=JSON.parse(require("node:fs").readFileSync(0,"utf8")).manifests;
const actual=list.map(item=>`${item.platform.os}/${item.platform.architecture}`).sort();
if (JSON.stringify(actual)!==JSON.stringify(process.argv[1].split(",").sort())) process.exit(1);
' "$platforms"
    # Exercise manifest resolution without --platform on the build host too.
    "$container_engine" run --rm --entrypoint node "$candidate" -e \
        'if (process.arch !== process.argv[1]) process.exit(1)' "$(node -p process.arch)"
    result=$candidate
else
    result=${images[0]}
fi
"$container_engine" tag "$result" "$image_name"
retained=true
if test "${#targets[@]}" -gt 1; then
    for index in "${!targets[@]}"; do
        "$container_engine" tag "${images[$index]}" "$image_name-${targets[$index]#linux/}"
    done
fi
if $push; then
    if test "${#targets[@]}" -gt 1; then
        "$container_engine" manifest push --all "$image_name" "docker://$registry_image:$version"
        "$container_engine" manifest push --all "$image_name" "docker://$registry_image:latest"
    else
        "$container_engine" push "$image_name" "docker://$registry_image:$version"
        "$container_engine" push "$image_name" "docker://$registry_image:latest"
    fi
fi
echo "Built and verified: $image_name ($platforms)"
