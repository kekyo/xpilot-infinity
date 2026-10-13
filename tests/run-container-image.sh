#!/bin/sh
set -eu

: "${XPILOT_CONTAINER_BUILDER:?Set the container builder}"
case $(uname -m) in
    x86_64|amd64) platform=linux/amd64;;
    aarch64|arm64) platform=linux/arm64;;
    *) echo 'Unsupported container architecture' >&2; exit 1;;
esac
image=localhost/xpilot-container-test:$$
cleanup() { "${CONTAINER_ENGINE:-podman}" image rm "$image" >/dev/null 2>&1 || true; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"$XPILOT_CONTAINER_BUILDER" --platform "$platform" --tag "$image"
