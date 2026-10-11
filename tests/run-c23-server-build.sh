#!/bin/sh

set -eu

fail()
{
    echo "run-c23-server-build.sh: $*" >&2
    exit 1
}

test -n "${XPILOT_C23_CC:-}" || fail "XPILOT_C23_CC is not set"
test -n "${XPILOT_C23_SOURCE_DIR:-}" \
    || fail "XPILOT_C23_SOURCE_DIR is not set"
test -n "${XPILOT_C23_BUILD_DIR:-}" \
    || fail "XPILOT_C23_BUILD_DIR is not set"

server_source="$XPILOT_C23_SOURCE_DIR/src/server/suibotdef.c"
test -f "$server_source" || fail "server source is unavailable: $server_source"
test -f "$XPILOT_C23_BUILD_DIR/config.h" \
    || fail "configured build header is unavailable"

c23_flag=
for candidate_flag in -std=gnu23 -std=gnu2x; do
    if printf '%s\n' 'int main(void) { return 0; }' \
        | $XPILOT_C23_CC "$candidate_flag" -x c -fsyntax-only - \
            >/dev/null 2>&1; then
        c23_flag=$candidate_flag
        break
    fi
done
test -n "$c23_flag" || fail "the configured compiler does not support C23"

# C23 string searches preserve const qualifiers in current libc headers.
# Include the client message parser and shared diagnostics in this check.
$XPILOT_C23_CC "$c23_flag" -Werror=discarded-qualifiers -fsyntax-only \
    -DHAVE_CONFIG_H \
    -DCONF_DATADIR='"/usr/share/games/xpilot-infinity/"' \
    -I"$XPILOT_C23_BUILD_DIR" \
    -I"$XPILOT_C23_SOURCE_DIR/src/server" \
    -I"$XPILOT_C23_SOURCE_DIR/src/client" \
    -I"$XPILOT_C23_BUILD_DIR/src/common" \
    -I"$XPILOT_C23_SOURCE_DIR/src/common" \
    "$server_source" \
    "$XPILOT_C23_SOURCE_DIR/src/common/error.c" \
    "$XPILOT_C23_SOURCE_DIR/src/server/option.c" \
    "$XPILOT_C23_SOURCE_DIR/src/client/messages.c"
