#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
container_builder=${XPILOT_CONTAINER_BUILDER:-$project_root/build_container_image.sh}
exec "$container_builder" --platform linux/amd64,linux/arm64 "$@"
