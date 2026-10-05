#!/bin/sh
set -eu
test "$(id -u)" -ne 0
test ! -w /etc/default/xpilot-infinity-server
case "$1" in */xpilot-infinity-server-editor/xpilot-infinity-server.txt) ;; *) exit 2 ;; esac
: "${XPILOT_EDITOR_TEST_PORT:?Set the expected test port}"
# Simulate a normal text editor's atomic save. This process does not apply it.
printf "XPILOT_SERVER_OPTIONS='-noQuit +reportMeta -port %s -map ndh.xp2'\n" \
    "$XPILOT_EDITOR_TEST_PORT" > "$1.saved"
mv "$1.saved" "$1"
