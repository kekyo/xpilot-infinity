#!/bin/sh
# Run only inside a newly created, rootless container with its own system bus.
set -eu

: "${XPILOT_SYSTEMD_TEST_IMAGE:?Set an image containing systemd, dbus and runtime libraries for the built binaries}"
: "${XPILOT_SERVICE_DRIVER:?Set the absolute service-driver path}"
: "${XPILOT_SERVER_BINARY:?Set the absolute server path}"
: "${XPILOT_CONTACT_TARGET_PROBE:?Set the absolute contact probe path}"
: "${XPILOT_TEST_SOURCE_DIR:?Set the absolute source directory}"
: "${XPILOT_TEST_PKGDATADIR:?Set the compiled server data directory}"
test "$(id -u)" -ne 0 || { echo 'Run this fixture as a rootless podman user' >&2; exit 1; }
case "$XPILOT_TEST_PKGDATADIR" in /*) ;; *) exit 2 ;; esac
container=
data_file=$(mktemp)
cleanup()
{
    if test -n "$container"; then
        podman cp "$container:/xpilot-test-log" - | tar -xO || true
        podman logs "$container" || true
        podman stop --time 10 "$container" >/dev/null 2>&1 || true
        podman rm "$container" >/dev/null || true
    fi
    rm -- "$data_file"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# The rootless user namespace bounds capabilities. systemd's production unit
# needs mount/device namespacing, which podman's default seccomp profile denies.
container=$(podman create --privileged --network=none --systemd=always \
    --entrypoint /sbin/init "$XPILOT_SYSTEMD_TEST_IMAGE" --unit=xpilot-tray-test.service)
if test -n "${XPILOT_TEST_PACKAGE:-}"; then
    podman cp "$XPILOT_TEST_PACKAGE" "$container:/xpilot-test.deb"
fi
# Install the test as a unit before boot. Its dependency on dbus.service is the
# readiness boundary; no fixed delay or periodic startup probe is involved.
podman cp "$XPILOT_SERVICE_DRIVER" "$container:/service-driver"
podman cp "$XPILOT_CONTACT_TARGET_PROBE" "$container:/contact-probe"
if test -z "${XPILOT_TEST_PACKAGE:-}"; then
    podman cp "$XPILOT_SERVER_BINARY" "$container:/usr/games/xpilot-infinity-server"
fi
if test -n "${XPILOT_TRAY_BINARY:-}"; then
    : "${XPILOT_DESKTOP_TEST:?Set the desktop test path}"
    if test -z "${XPILOT_TEST_PACKAGE:-}"; then
        podman cp "$XPILOT_TRAY_BINARY" "$container:/xpilot-infinity-tray"
    fi
    podman cp "$XPILOT_DESKTOP_TEST" "$container:/test-desktop"
    podman cp "$XPILOT_TEST_SOURCE_DIR/images/icon-1254.png" "$container:/xpilot-test-icon.png"
fi
podman cp "$XPILOT_TEST_SOURCE_DIR/lib" "$container:/xpilot-test-data"
podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/test-strict-map.sh" "$container:/test-strict-map.sh"
if test -n "${XPILOT_SETTINGS_HELPER:-}"; then
    : "${XPILOT_AUTH_CANCEL_TEST:?Set the authentication-cancel test path}"
    podman cp "$XPILOT_AUTH_CANCEL_TEST" "$container:/test-auth-cancel"
    podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/test-default-editor.sh" "$container:/test-default-editor"
    if test -z "${XPILOT_TEST_PACKAGE:-}"; then
        podman cp "$XPILOT_SETTINGS_HELPER" "$container:/xpilot-settings-helper"
        podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/data/org.xpilot.infinity.policy" "$container:/usr/share/polkit-1/actions/org.xpilot.infinity.policy"
        podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/data/org.xpilot.Infinity.ServerSettings1.conf" "$container:/etc/dbus-1/system.d/org.xpilot.Infinity.ServerSettings1.conf"
    fi
    podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/test-settings-editor.sh" "$container:/test-settings-editor.sh"
    podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/test-settings-helper.sh" "$container:/test-settings-helper.sh"
fi
podman cp "$XPILOT_TEST_SOURCE_DIR/debian/xpilot-infinity-server.service" \
    "$container:/xpilot-test-server.service"
if test -z "${XPILOT_TEST_PACKAGE:-}"; then
    podman cp "$XPILOT_TEST_SOURCE_DIR/debian/xpilot-infinity-server.default" \
        "$container:/etc/default/xpilot-infinity-server"
fi
podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/run-systemd-inside.sh" \
    "$container:/run-systemd-inside.sh"
podman cp "$XPILOT_TEST_SOURCE_DIR/src/tray/tests/xpilot-tray-test.service" \
    "$container:/etc/systemd/system/xpilot-tray-test.service"
# A tar stream avoids shell interpolation of the compiled data directory.
printf '%s\n' "$XPILOT_TEST_PKGDATADIR" > "$data_file"
podman cp "$data_file" "$container:/xpilot-data-directory"
podman start "$container" >/dev/null
podman wait "$container"
result=$(podman cp "$container:/xpilot-test-result" - | tar -xO)
test "$result" = 0
echo 'Real systemd service controls passed'
