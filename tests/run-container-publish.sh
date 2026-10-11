#!/bin/sh

set -eu

fail()
{
    echo "run-container-publish.sh: $*" >&2
    exit 1
}

test -n "${XPILOT_CONTAINER_PUBLISHER:-}" \
    || fail "XPILOT_CONTAINER_PUBLISHER is not set"
test -x "$XPILOT_CONTAINER_PUBLISHER" \
    || fail "container publish script is unavailable: $XPILOT_CONTAINER_PUBLISHER"

test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-container-publish-test.XXXXXX")

cleanup()
{
    case $test_root in
        "${TMPDIR:-/tmp}"/xpilot-container-publish-test.*)
            rm -rf -- "$test_root"
            ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

invocation_log=$test_root/invocations.log
expected_log=$test_root/expected.log
fixture_bin=$test_root/bin
fixture_builder=$test_root/build_container_image.sh
mkdir -p "$fixture_bin"

cat > "$fixture_builder" <<'EOF'
#!/bin/sh

set -eu

if test "$#" -eq 1 && test "$1" = --print-version; then
    printf '%s\n' resolve-version >> "$XPILOT_TEST_INVOCATION_LOG"
    printf '%s\n' 7.8.9
    exit 0
fi

{
    printf '%s\n' builder
    for argument in "$@"; do
        printf '%s\n' "$argument"
    done
} >> "$XPILOT_TEST_INVOCATION_LOG"
EOF

cat > "$fixture_bin/git" <<'EOF'
#!/bin/sh

set -eu

case $* in
    "rev-parse HEAD"|-C*" rev-parse HEAD")
        printf '%s\n' git-revision >> "$XPILOT_TEST_INVOCATION_LOG"
        printf '%s\n' 0123456701234567012345670123456701234567
        ;;
    *)
        echo "unexpected git invocation: $*" >&2
        exit 1
        ;;
esac
EOF

cat > "$fixture_bin/screw-up" <<'EOF'
#!/bin/sh

set -eu

test "$#" -eq 2 && test "$1" = format && test "$2" = --force \
    || { echo "unexpected screw-up invocation: $*" >&2; exit 1; }
IFS= read -r version_template
IFS= read -r revision_template
test "$version_template" = '{version}' \
    || { echo "unexpected version template: $version_template" >&2; exit 1; }
test "$revision_template" = '{git.commit.hash}' \
    || { echo "unexpected revision template: $revision_template" >&2; exit 1; }

printf '%s\n' resolve-metadata >> "$XPILOT_TEST_INVOCATION_LOG"
printf '%s\n' 7.8.9 fedcba9876543210fedcba9876543210fedcba98
EOF

cat > "$fixture_bin/container-engine" <<'EOF'
#!/bin/sh

set -eu

{
    printf '%s\n' container-engine
    for argument in "$@"; do
        printf '%s\n' "$argument"
    done
} >> "$XPILOT_TEST_INVOCATION_LOG"
EOF

chmod +x "$fixture_builder" "$fixture_bin/git" "$fixture_bin/screw-up" \
    "$fixture_bin/container-engine"
ln -s container-engine "$fixture_bin/podman"

(
    cd "$test_root"
    PATH="$fixture_bin:$PATH" \
    SCREW_UP=$fixture_bin/screw-up \
    XPILOT_VERSION= \
    XPILOT_COMMIT_ID= \
    XPILOT_CONTAINER_BUILDER=$fixture_builder \
    CONTAINER_ENGINE=$fixture_bin/container-engine \
    XPILOT_TEST_INVOCATION_LOG=$invocation_log \
        "$XPILOT_CONTAINER_PUBLISHER"
)

cat > "$expected_log" <<'EOF'
resolve-metadata
builder
--version
7.8.9
--revision
fedcba9876543210fedcba9876543210fedcba98
--platform
linux/amd64,linux/arm64
--tag
localhost/xpilot-infinity-server:7.8.9-multi
container-engine
manifest
push
--all
localhost/xpilot-infinity-server:7.8.9-multi
docker://docker.io/kekyo/xpilot-infinity-server:7.8.9
container-engine
manifest
push
--all
localhost/xpilot-infinity-server:7.8.9-multi
docker://docker.io/kekyo/xpilot-infinity-server:latest
EOF

diff -u "$expected_log" "$invocation_log" \
    || fail "the resolved screw-up metadata was not used for the image build"
