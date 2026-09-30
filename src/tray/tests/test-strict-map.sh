#!/bin/sh
set -eu
: "${XPILOT_SERVER_BINARY:?Set the server executable}"
scratch=$(mktemp -d)
cleanup()
{
    rm -f -- "$scratch/broken.xp2" "$scratch/server.log"
    rmdir -- "$scratch"
}
trap cleanup EXIT HUP INT TERM
printf '%s\n' '<XPilotMap><broken' > "$scratch/broken.xp2"
for map in "$scratch/missing.xp2" "$scratch/broken.xp2"; do
    result=0
    timeout "${XPILOT_STRICT_TEST_TIMEOUT:-30}" "$XPILOT_SERVER_BINARY" \
        -strictMap -map "$map" +reportMeta -noQuit > "$scratch/server.log" 2>&1 || result=$?
    if test "$result" != 1; then
        cat "$scratch/server.log"
        echo "Unreadable required map did not fail startup (exit=$result)" >&2
        exit 1
    fi
done
