#!/bin/sh

set -eu

fail()
{
    echo "run-quadlet.sh: $*" >&2
    exit 1
}

skip()
{
    echo "SKIP: $*" >&2
    exit 77
}

test -n "${XPILOT_QUADLET_DIR:-}" \
    || fail "XPILOT_QUADLET_DIR is not set"

quadlet_dir=$XPILOT_QUADLET_DIR
generator=${PODMAN_SYSTEM_GENERATOR:-/usr/lib/systemd/system-generators/podman-system-generator}

test -x "$generator" || skip "the Podman system generator is unavailable"
test -f "$quadlet_dir/xpilot-infinity-server.container" \
    || fail "the server Quadlet is unavailable"
test -f "$quadlet_dir/xpilot-infinity-server-data.volume" \
    || fail "the server data volume Quadlet is unavailable"

generated_units=$(QUADLET_UNIT_DIRS=$quadlet_dir \
    "$generator" --user --dryrun 2>&1) \
    || fail "the Podman system generator rejected the server Quadlet"

printf '%s\n' "$generated_units" \
    | grep -F 'xpilot-infinity-server.service' >/dev/null 2>&1 \
    || fail "the server Quadlet did not generate a systemd service"
printf '%s\n' "$generated_units" \
    | grep -F 'xpilot-infinity-server-data-volume.service' >/dev/null 2>&1 \
    || fail "the data volume Quadlet did not generate a systemd service"

printf '%s\n' "$generated_units" | grep -F -- '--publish 15347:15347/tcp' >/dev/null \
    || fail "the service does not publish the browser port"
if printf '%s\n' "$generated_units" | grep -E -- '--publish [^ ]*(15345|15346|/udp)' >/dev/null; then
    fail "the default service publishes native game ports"
fi
echo "Podman Quadlet generation checks passed"
