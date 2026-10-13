#!/bin/sh
set -eu
: "${XPILOT_CONTAINER_PUBLISHER:?Set the container build wrapper}"
test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-container-wrapper.XXXXXX")
trap 'rm -rf -- "$test_root"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cat > "$test_root/builder" <<'BUILDER'
#!/bin/sh
printf '%s\n' "$@" > "$XPILOT_TEST_INVOCATION_LOG"
exit "${XPILOT_TEST_EXIT_CODE:-0}"
BUILDER
chmod +x "$test_root/builder"
export XPILOT_CONTAINER_BUILDER=$test_root/builder
export XPILOT_TEST_INVOCATION_LOG=$test_root/calls
"$XPILOT_CONTAINER_PUBLISHER"
printf '%s\n' --platform linux/amd64,linux/arm64 > "$test_root/expected"
diff -u "$test_root/expected" "$test_root/calls"
"$XPILOT_CONTAINER_PUBLISHER" --push --version 9.8.7
printf '%s\n' --platform linux/amd64,linux/arm64 --push --version 9.8.7 > "$test_root/expected"
diff -u "$test_root/expected" "$test_root/calls"
if XPILOT_TEST_EXIT_CODE=17 "$XPILOT_CONTAINER_PUBLISHER"; then
    echo 'The wrapper ignored the build failure' >&2
    exit 1
else
    test "$?" -eq 17
fi
