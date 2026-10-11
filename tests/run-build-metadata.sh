#!/bin/sh

set -eu

fail()
{
    echo "run-build-metadata.sh: $*" >&2
    exit 1
}

test -n "${XPILOT_BUILD_METADATA_RESOLVER:-}" \
    || fail "XPILOT_BUILD_METADATA_RESOLVER is not set"
test -x "$XPILOT_BUILD_METADATA_RESOLVER" \
    || fail "build metadata resolver is unavailable: $XPILOT_BUILD_METADATA_RESOLVER"

test_root=$(mktemp -d \
    "${TMPDIR:-/tmp}/xpilot-build-metadata-test.XXXXXX")

cleanup()
{
    case $test_root in
        "${TMPDIR:-/tmp}"/xpilot-build-metadata-test.*)
            rm -rf -- "$test_root"
            ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

fixture_screw_up="$test_root/screw-up"
fixture_log="$test_root/screw-up.log"
expected_commit=0123456701234567012345670123456701234567

cat > "$fixture_screw_up" <<'EOF'
#!/bin/sh
set -eu

test "$#" -eq 2
test "$1" = format
test "$2" = --force
printf 'invoked\n' >> "$XPILOT_BUILD_METADATA_TEST_LOG"
template=$(cat)
test "$template" = "{version}
{git.commit.hash}"
printf '%s\n' 7.8.9 0123456701234567012345670123456701234567
EOF
chmod +x "$fixture_screw_up"

resolved_metadata=$(SCREW_UP="$fixture_screw_up" \
    XPILOT_BUILD_METADATA_TEST_LOG="$fixture_log" \
    XPILOT_VERSION= XPILOT_COMMIT_ID= \
    "$XPILOT_BUILD_METADATA_RESOLVER")
test "$resolved_metadata" = "7.8.9
$expected_commit" \
    || fail "version and commit ID were not resolved together: $resolved_metadata"
test "$(wc -l < "$fixture_log" | tr -d ' ')" -eq 1 \
    || fail "screw-up was not invoked exactly once"

override_commit=89abcdef89abcdef89abcdef89abcdef89abcdef
resolved_override=$(SCREW_UP="$test_root/missing-screw-up" \
    XPILOT_VERSION=9.8.7 XPILOT_COMMIT_ID="$override_commit" \
    "$XPILOT_BUILD_METADATA_RESOLVER")
test "$resolved_override" = "9.8.7
$override_commit" \
    || fail "resolved metadata overrides were not preserved"

if XPILOT_VERSION=9.8.7 XPILOT_COMMIT_ID=1234567 \
    "$XPILOT_BUILD_METADATA_RESOLVER" >/dev/null 2>&1
then
    fail "a shortened commit ID was accepted"
fi

echo "Build metadata resolution passed"
