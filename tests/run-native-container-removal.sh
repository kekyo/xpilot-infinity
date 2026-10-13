#!/bin/sh
set -eu
: "${XPILOT_CONTAINER_IMAGE:?Set the image under test}"
podman image exists "$XPILOT_CONTAINER_IMAGE"
output=$(podman run --pull=never --rm --entrypoint /usr/games/xpilot-infinity-server "$XPILOT_CONTAINER_IMAGE" -version 2>&1 || true)
if printf '%s\n' "$output" | grep -F 'XPilot Infinity ['; then
    echo 'The obsolete native container executable is still available' >&2
    exit 1
fi
