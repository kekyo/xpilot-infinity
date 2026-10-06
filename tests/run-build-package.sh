#!/bin/sh

set -eu

fail()
{
    echo "run-build-package.sh: $*" >&2
    exit 1
}

assert_equal()
{
    expected=$1
    actual=$2
    message=$3

    test "$expected" = "$actual" \
        || fail "$message (expected '$expected', got '$actual')"
}

assert_contains()
{
    target_path=$1
    expected_text=$2

    grep -F -- "$expected_text" "$target_path" >/dev/null 2>&1 \
        || fail "$target_path does not contain: $expected_text"
}

assert_not_contains()
{
    target_path=$1
    unexpected_text=$2

    if grep -F -- "$unexpected_text" "$target_path" >/dev/null 2>&1; then
        fail "$target_path unexpectedly contains: $unexpected_text"
    fi
}

test -n "${XPILOT_BUILD_PACKAGE:-}" \
    || fail "XPILOT_BUILD_PACKAGE is not set"
test -f "$XPILOT_BUILD_PACKAGE" \
    || fail "package build script is unavailable: $XPILOT_BUILD_PACKAGE"

package_script_dir=$(CDPATH= cd -- "$(dirname -- "$XPILOT_BUILD_PACKAGE")" \
    && pwd)
package_script="$package_script_dir/$(basename -- "$XPILOT_BUILD_PACKAGE")"
package_all_script="$package_script_dir/build_package_all.sh"
prereq_script="$package_script_dir/prereq.sh"
linux_dist_script="$package_script_dir/scripts/build_linux_dist_container.sh"
test_commit=0123456701234567012345670123456701234567

test -x "$package_all_script" \
    || fail "complete package build script is unavailable: $package_all_script"
test -x "$prereq_script" \
    || fail "prerequisite image script is unavailable: $prereq_script"
test -f "$linux_dist_script" \
    || fail "Linux distribution script is unavailable: $linux_dist_script"

BUILD_PACKAGE_PROJECT_ROOT=$package_script_dir
BUILD_PACKAGE_SOURCE_ONLY=1
export BUILD_PACKAGE_PROJECT_ROOT BUILD_PACKAGE_SOURCE_ONLY
. "$XPILOT_BUILD_PACKAGE"
unset BUILD_PACKAGE_SOURCE_ONLY BUILD_PACKAGE_PROJECT_ROOT

assert_equal x86_64 "$(canonical_arch amd64)" \
    "amd64 architecture alias was not normalized"
assert_equal i686 "$(canonical_arch i386)" \
    "i386 architecture alias was not normalized"
assert_equal arm64 "$(canonical_arch aarch64)" \
    "aarch64 architecture alias was not normalized"
assert_equal armv7l "$(canonical_arch armhf)" \
    "armhf architecture alias was not normalized"
assert_equal 24.04 "$(canonical_release noble)" \
    "Ubuntu noble release alias was not normalized"
assert_debian_dependency \
    "libc6 (>= 2.34), libalut0 (>= 1.1.0), libopenal1 (>= 1.14)" \
    libalut0
assert_debian_dependency \
    "libc6 (>= 2.34), libalut0 (>= 1.1.0), libopenal1 (>= 1.14)" \
    libopenal1
if (assert_debian_dependency "libc6 (>= 2.34)" libopenal1) \
    >/dev/null 2>&1
then
    fail "a missing Debian dependency was accepted"
fi
assert_equal "Kouji Matsui <k@kekyo.net>" "$DEFAULT_MAINTAINER" \
    "the default Debian maintainer was incorrect"
assert_equal 4.7.99-1 "$(debian_package_version 4.7.99 1)" \
    "the Debian package revision was not appended"

DISTRO_FILTER=
RELEASE_FILTER=
ARCH_FILTER=
assert_equal 15 "$(count_deb_builds)" \
    "the complete Linux package matrix was not selected"

DISTRO_FILTER=$(normalize_filter_list distro debian)
RELEASE_FILTER=$(normalize_filter_list release trixie)
ARCH_FILTER=$(normalize_filter_list arch riscv64)
assert_equal 1 "$(count_deb_builds)" \
    "a single Linux package target was not selected"

UPSTREAM_VERSION=4.7.99
DEBIAN_REVISION=1
DEBIAN_VERSION=$(debian_package_version \
    "$UPSTREAM_VERSION" "$DEBIAN_REVISION")
DEB_ARTIFACT_ROOT=/tmp/xpilot-artifacts
assert_equal \
    /tmp/xpilot-artifacts/xpilot-infinity-4.7.99-1-ubuntu-24.04-amd64.deb \
    "$(deb_artifact_path ubuntu 24.04 x86_64)" \
    "the Debian artifact name was incorrect"
assert_equal \
    localhost/xpilot-infinity-pack-deb-debian-bookworm-x86_64:latest \
    "$(prereq_image_for_target debian bookworm x86_64)" \
    "the prerequisite image name was incorrect"

test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-package-test.XXXXXX")

cleanup()
{
    case "$test_root" in
        "${TMPDIR:-/tmp}"/xpilot-package-test.*)
            rm -rf -- "$test_root"
            ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

version_resolver_fixture="$test_root/version-resolver"
cat > "$version_resolver_fixture" <<'EOF'
#!/bin/sh
printf '%s\n' 7.8.9 0123456701234567012345670123456701234567
EOF
chmod +x "$version_resolver_fixture"
assert_equal 7.8.9 \
    "$(XPILOT_BUILD_METADATA_RESOLVER=$version_resolver_fixture \
        "$package_script" --print-version)" \
    "the package version was not resolved at build time"

policy_source="$test_root/policy-source"
policy_stage="$test_root/policy-stage"
policy_meta="$test_root/policy-meta"
mkdir -p "$policy_source/debian" \
    "$policy_source/images" \
    "$policy_stage/usr/games" "$policy_stage/usr/share/man/man6" \
    "$policy_stage/usr/lib/xpilot-infinity" \
    "$policy_meta"
printf 'fixture README\n' > "$policy_source/README.md"
printf 'fixture README ja\n' > "$policy_source/README_ja.md"
printf 'fixture upstream changes\n' > "$policy_source/ChangeLog"
printf 'fixture copyright\n' > "$policy_source/debian/copyright"
cat > "$policy_source/debian/changelog.in" <<'EOF'
xpilot-infinity (@XPILOT_PACKAGE_VERSION@) unstable; urgency=medium

  * Fixture package release.

 -- Kouji Matsui <k@kekyo.net>  Fri, 28 Aug 2026 14:32:29 +0900
EOF
for package_file in \
    xpilot-infinity-server.service xpilot-infinity-server.default \
    xpilot-infinity.postinst xpilot-infinity.prerm xpilot-infinity.postrm \
    xpilot-infinity.desktop
do
    test -f "$package_script_dir/debian/$package_file" \
        || fail "Debian packaging file is unavailable: $package_file"
    cp "$package_script_dir/debian/$package_file" \
        "$policy_source/debian/$package_file"
done
test -f "$package_script_dir/images/icon-1254.png" \
    || fail "Debian application icon is unavailable"
cp "$package_script_dir/images/icon-1254.png" \
    "$policy_source/images/icon-1254.png"
printf '.TH XPILOT 6\n' \
    > "$policy_stage/usr/share/man/man6/xpilot-infinity-sdl.6"
cat > "$test_root/fixture-executable.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *launch_log = getenv("XPILOT_DESKTOP_LAUNCH_LOG");

    (void)argv;
    if (launch_log != NULL) {
        FILE *stream = fopen(launch_log, "w");

        if (stream == NULL)
            return 1;
        fprintf(stream, "%d\n", argc);
        fclose(stream);
    }
    return 0;
}
EOF
cc -g "$test_root/fixture-executable.c" -o "$test_root/fixture-executable"
readelf -S "$test_root/fixture-executable" | grep -Fq .debug_info \
    || fail "the fixture executable did not contain debug information"
for executable_name in \
    xpilot-infinity-sdl xpilot-infinity-x11 xpilot-infinity-server \
    xpilot-infinity-replay xpilot-infinity-xp-mapedit xpilot-infinity-tray
do
    cp "$test_root/fixture-executable" \
        "$policy_stage/usr/games/$executable_name"
done
cp "$test_root/fixture-executable" \
    "$policy_stage/usr/lib/xpilot-infinity/xpilot-infinity-settings-helper"

(
    BUILD_LINUX_DIST_SOURCE_ONLY=1
    export BUILD_LINUX_DIST_SOURCE_ONLY
    . "$linux_dist_script"
    unset BUILD_LINUX_DIST_SOURCE_ONLY

    source_dir=$policy_source
    stage_dir=$policy_stage
    meta_dir=$policy_meta
    XPILOT_PACKAGE_NAME=xpilot-infinity
    XPILOT_PACKAGE_VERSION=4.7.99-1
    XPILOT_PACKAGE_DESCRIPTION="Fixture package"
    XPILOT_PACKAGE_MAINTAINER="Kouji Matsui <k@kekyo.net>"
    printf 'TODO: incomplete copyright\n' \
        > "$test_root/incomplete-copyright"
    if (validate_copyright_file "$test_root/incomplete-copyright") \
        >/dev/null 2>&1
    then
        fail "an incomplete copyright file was accepted"
    fi
    prepare_debian_package_files
    deb_arch=amd64
    write_control_file "libc6 (>= 2.35)"
)

test ! -e "$policy_stage/usr/share/man/man6/xpilot-infinity-sdl.6" \
    || fail "the uncompressed manual page was retained"
test -f "$policy_stage/usr/share/man/man6/xpilot-infinity-sdl.6.gz" \
    || fail "the compressed manual page was not installed"
assert_equal '.TH XPILOT 6' \
    "$(gzip -cd "$policy_stage/usr/share/man/man6/xpilot-infinity-sdl.6.gz")" \
    "the compressed manual page contents changed"
doc_dir="$policy_stage/usr/share/doc/xpilot-infinity"
assert_equal "fixture copyright" "$(cat "$doc_dir/copyright")" \
    "the Debian copyright file was not installed"
assert_equal "fixture upstream changes" \
    "$(gzip -cd "$doc_dir/changelog.gz")" \
    "the upstream changelog was not installed"
gzip -cd "$doc_dir/changelog.Debian.gz" \
    > "$test_root/installed-debian-changelog"
assert_contains "$test_root/installed-debian-changelog" \
    "xpilot-infinity (4.7.99-1) unstable"
test ! -e "$doc_dir/INSTALL" \
    || fail "source installation instructions were included"
if readelf -S "$policy_stage/usr/games/xpilot-infinity-server" \
    | grep -Fq .debug_info
then
    fail "the staged executable retained debug information"
fi
if readelf -S "$policy_stage/usr/games/xpilot-infinity-server" \
    | grep -Fq .symtab
then
    fail "the staged executable retained its static symbol table"
fi
assert_contains "$policy_stage/DEBIAN/control" "Version: 4.7.99-1"
assert_contains "$policy_stage/DEBIAN/control" \
    "Maintainer: Kouji Matsui <k@kekyo.net>"
assert_contains "$policy_stage/DEBIAN/control" \
    "Pre-Depends: init-system-helpers (>= 1.54~)"

desktop_entry="$policy_stage/usr/share/applications/xpilot-infinity.desktop"
desktop_icon="$policy_stage/usr/share/pixmaps/xpilot-infinity.png"
test -f "$desktop_entry" \
    || fail "the Debian desktop entry was not installed"
test -f "$desktop_icon" \
    || fail "the Debian desktop icon was not installed"
if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$desktop_entry" \
        || fail "the Debian desktop entry failed validation"
fi
desktop_command=$(sed -n 's/^Exec=//p' "$desktop_entry")
case $desktop_command in
    /usr/games/*) ;;
    *) fail "the desktop entry does not launch an installed game" ;;
esac
desktop_launch_log="$test_root/desktop-launch.log"
XPILOT_DESKTOP_LAUNCH_LOG=$desktop_launch_log \
    "$policy_stage$desktop_command"
assert_equal 1 "$(cat "$desktop_launch_log")" \
    "the desktop entry passed arguments to the SDL client"

service_unit="$policy_stage/usr/lib/systemd/system/xpilot-infinity-server.service"
service_defaults="$policy_stage/etc/default/xpilot-infinity-server"
assert_contains "$service_unit" "Type=exec"
assert_contains "$service_unit" "DynamicUser=yes"
assert_contains "$service_unit" \
    "EnvironmentFile=-/etc/default/xpilot-infinity-server"
assert_contains "$service_unit" \
    'ExecStart=/usr/games/xpilot-infinity-server $XPILOT_SERVER_OPTIONS'
assert_contains "$service_defaults" \
    'XPILOT_SERVER_OPTIONS="-noQuit +reportMeta -map ndh.xp2"'
assert_contains "$policy_stage/DEBIAN/conffiles" \
    "/etc/default/xpilot-infinity-server"
if command -v systemd-analyze >/dev/null 2>&1; then
    systemd-analyze verify --recursive-errors=no --root="$policy_stage" \
        xpilot-infinity-server.service >/dev/null 2>&1 \
        || fail "the staged systemd service failed validation"
fi
for maintainer_script in postinst prerm postrm; do
    test -x "$policy_stage/DEBIAN/$maintainer_script" \
        || fail "the $maintainer_script maintainer script was not executable"
done

systemd_fixture_tools="$test_root/systemd-tools"
systemd_fixture_log="$test_root/systemd-maintainer.log"
mkdir -p "$systemd_fixture_tools"
cat > "$systemd_fixture_tools/deb-systemd-helper" <<'EOF'
#!/bin/sh
printf 'deb-systemd-helper:%s\n' "$*" >> "$XPILOT_SYSTEMD_FIXTURE_LOG"
case $1 in
    debian-installed) exit "${XPILOT_DEBIAN_INSTALLED:-1}" ;;
    --quiet)
        test "${2:-}" = was-enabled || exit 0
        exit "${XPILOT_WAS_ENABLED:-1}"
        ;;
esac
exit 0
EOF
cat > "$systemd_fixture_tools/deb-systemd-invoke" <<'EOF'
#!/bin/sh
printf 'deb-systemd-invoke:%s\n' "$*" >> "$XPILOT_SYSTEMD_FIXTURE_LOG"
EOF
cat > "$systemd_fixture_tools/systemctl" <<'EOF'
#!/bin/sh
printf 'systemctl:%s\n' "$*" >> "$XPILOT_SYSTEMD_FIXTURE_LOG"
EOF
chmod +x "$systemd_fixture_tools/deb-systemd-helper" \
    "$systemd_fixture_tools/deb-systemd-invoke" \
    "$systemd_fixture_tools/systemctl"

: > "$systemd_fixture_log"
PATH="$systemd_fixture_tools:$PATH" \
XPILOT_SYSTEMD_FIXTURE_LOG="$systemd_fixture_log" \
XPILOT_DEBIAN_INSTALLED=1 XPILOT_WAS_ENABLED=1 \
    "$policy_stage/DEBIAN/postinst" configure
assert_contains "$systemd_fixture_log" \
    "deb-systemd-helper:update-state xpilot-infinity-server.service"
assert_not_contains "$systemd_fixture_log" "deb-systemd-helper:enable"
assert_not_contains "$systemd_fixture_log" "deb-systemd-invoke:start"
assert_not_contains "$systemd_fixture_log" "deb-systemd-invoke:try-restart"

if test -d /run/systemd/system; then
    : > "$systemd_fixture_log"
    PATH="$systemd_fixture_tools:$PATH" \
    XPILOT_SYSTEMD_FIXTURE_LOG="$systemd_fixture_log" \
    XPILOT_DEBIAN_INSTALLED=0 XPILOT_WAS_ENABLED=0 \
        "$policy_stage/DEBIAN/postinst" configure 4.7.98-1
    assert_contains "$systemd_fixture_log" \
        "deb-systemd-helper:enable xpilot-infinity-server.service"
    assert_contains "$systemd_fixture_log" \
        "deb-systemd-invoke:try-restart xpilot-infinity-server.service"

    : > "$systemd_fixture_log"
    PATH="$systemd_fixture_tools:$PATH" \
    XPILOT_SYSTEMD_FIXTURE_LOG="$systemd_fixture_log" \
        "$policy_stage/DEBIAN/prerm" remove
    assert_contains "$systemd_fixture_log" \
        "deb-systemd-invoke:stop xpilot-infinity-server.service"
fi

: > "$systemd_fixture_log"
PATH="$systemd_fixture_tools:$PATH" \
XPILOT_SYSTEMD_FIXTURE_LOG="$systemd_fixture_log" \
    "$policy_stage/DEBIAN/postrm" purge
assert_contains "$systemd_fixture_log" \
    "deb-systemd-helper:purge xpilot-infinity-server.service"

fixture_project="$test_root/project"
fixture_tools="$test_root/tools"
fixture_log="$test_root/container.log"
fixture_dpkg_log="$test_root/dpkg.log"
mkdir -p "$fixture_project/scripts" "$fixture_tools"

cat > "$fixture_tools/container-engine" <<'EOF'
#!/bin/sh
set -eu

if test "${1:-}" = image && test "${2:-}" = exists; then
    printf 'exists=%s\n' "${3:-}" >> "$XPILOT_PACKAGE_CONTAINER_LOG"
    exit 0
fi

test "${1:-}" = run || exit 2
workspace=
work_dir=
previous=
for argument do
    if test "$previous" = volume; then
        workspace=${argument%%:/workspace*}
        previous=
        continue
    fi
    if test "$previous" = environment; then
        printf 'environment=%s\n' "$argument" \
            >> "$XPILOT_PACKAGE_CONTAINER_LOG"
        case "$argument" in
            XPILOT_WORK_DIR=*) work_dir=${argument#*=} ;;
        esac
        previous=
        continue
    fi
    if test "$previous" = platform; then
        printf 'platform=%s\n' "$argument" \
            >> "$XPILOT_PACKAGE_CONTAINER_LOG"
        previous=
        continue
    fi
    case "$argument" in
        -v) previous=volume ;;
        -e) previous=environment ;;
        --platform) previous=platform ;;
        localhost/*) printf 'run-image=%s\n' "$argument" \
            >> "$XPILOT_PACKAGE_CONTAINER_LOG" ;;
    esac
done

test -n "$workspace"
test -n "$work_dir"
host_work_dir="$workspace/${work_dir#/workspace/}"
mkdir -p "$host_work_dir/stage/xpilot-infinity/DEBIAN"
printf 'Package: xpilot-infinity\n' \
    > "$host_work_dir/stage/xpilot-infinity/DEBIAN/control"
EOF

cat > "$fixture_tools/dpkg-deb" <<'EOF'
#!/bin/sh
set -eu

output_path=
for argument do
    output_path=$argument
done
mkdir -p "$(dirname -- "$output_path")"
printf 'fixture deb\n' > "$output_path"
printf '%s\n' "$*" >> "$XPILOT_PACKAGE_DPKG_LOG"
EOF

chmod +x "$fixture_tools/container-engine" "$fixture_tools/dpkg-deb"

PROJECT_ROOT=$fixture_project
ARTIFACT_ROOT="$fixture_project/artifacts"
DEB_ARTIFACT_ROOT="$ARTIFACT_ROOT/deb"
RUN_ID=test-run
TMP_ROOT="$ARTIFACT_ROOT/.tmp/$RUN_ID"
UPSTREAM_VERSION=4.7.99
BUILD_COMMIT_ID=$test_commit
DEBIAN_REVISION=1
DEBIAN_VERSION=$(debian_package_version \
    "$UPSTREAM_VERSION" "$DEBIAN_REVISION")
CONTAINER_ENGINE_BIN="$fixture_tools/container-engine"
MAKE_JOBS=3
BUILD_TYPE=Release
export XPILOT_PACKAGE_CONTAINER_LOG=$fixture_log
export XPILOT_PACKAGE_DPKG_LOG=$fixture_dpkg_log
PATH="$fixture_tools:$PATH"
export PATH

build_deb_package debian bookworm x86_64 linux/amd64

assert_contains "$fixture_log" \
    "exists=localhost/xpilot-infinity-pack-deb-debian-bookworm-x86_64:latest"
assert_contains "$fixture_log" "platform=linux/amd64"
assert_contains "$fixture_log" "environment=XPILOT_VERSION=4.7.99"
assert_contains "$fixture_log" "environment=XPILOT_COMMIT_ID=$test_commit"
assert_contains "$fixture_log" "environment=XPILOT_PACKAGE_VERSION=4.7.99-1"
assert_contains "$fixture_log" \
    "environment=XPILOT_PACKAGE_MAINTAINER=Kouji Matsui <k@kekyo.net>"
assert_contains "$fixture_log" \
    "run-image=localhost/xpilot-infinity-pack-deb-debian-bookworm-x86_64:latest"
assert_contains "$fixture_dpkg_log" \
    "$DEB_ARTIFACT_ROOT/xpilot-infinity-4.7.99-1-debian-bookworm-amd64.deb"
test -f "$DEB_ARTIFACT_ROOT/xpilot-infinity-4.7.99-1-debian-bookworm-amd64.deb" \
    || fail "the Debian artifact was not created"

# A failed parallel job must retain both output streams and identify its target
# after the other jobs finish, instead of ending with an unrelated job's output.
parallel_driver="$test_root/parallel-driver"
cat > "$parallel_driver" <<'EOF'
#!/bin/sh
set -eu
BUILD_PACKAGE_SOURCE_ONLY=1
. "$XPILOT_BUILD_PACKAGE"
PACKAGE_LOG_ROOT="$XPILOT_PARALLEL_TEST_ROOT/logs"
PARALLEL_JOBS=2
ACTIVE_JOB_PIDS=
ACTIVE_JOB_NAMES=
ACTIVE_JOB_COUNT=0
JOB_FAILURE=0
FAILED_JOBS=
run_parallel_job debian/bookworm/arm64 sh -c \
    'echo "configure output"; echo "font dependency discovery failed" >&2; exit 23'
run_parallel_job ubuntu/26.04/arm64 sh -c 'echo "successful package output"'
wait_for_all_jobs
EOF
chmod +x "$parallel_driver"
if XPILOT_BUILD_PACKAGE="$package_script" \
    XPILOT_PARALLEL_TEST_ROOT="$test_root/parallel" \
    "$parallel_driver" > "$test_root/parallel-output" 2>&1
then
    fail "a failed parallel package build was accepted"
fi
assert_contains "$test_root/parallel-output" "debian/bookworm/arm64"
assert_contains "$test_root/parallel-output" "exit 23"
assert_contains "$test_root/parallel-output" "font dependency discovery failed"
assert_contains "$test_root/parallel-output" \
    "$test_root/parallel/logs/debian/bookworm/arm64.log"
assert_contains "$test_root/parallel/logs/debian/bookworm/arm64.log" \
    "configure output"
assert_contains "$test_root/parallel/logs/debian/bookworm/arm64.log" \
    "font dependency discovery failed"
assert_contains "$test_root/parallel/logs/ubuntu/26.04/arm64.log" \
    "successful package output"

all_args="$test_root/build-all.args"
windows_args="$test_root/build-windows.args"
cat > "$fixture_tools/build-package-stub" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$@" > "$XPILOT_PACKAGE_ALL_ARGS"
printf '%s\n' "$XPILOT_COMMIT_ID" > "$XPILOT_PACKAGE_ALL_ARGS.commit"
EOF
cat > "$fixture_tools/build-windows-stub" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$@" > "$XPILOT_WINDOWS_PACKAGE_ARGS"
printf '%s\n' "$XPILOT_COMMIT_ID" > "$XPILOT_WINDOWS_PACKAGE_ARGS.commit"
EOF
chmod +x "$fixture_tools/build-package-stub" \
    "$fixture_tools/build-windows-stub"

XPILOT_PACKAGE_ALL_ARGS=$all_args \
XPILOT_WINDOWS_PACKAGE_ARGS=$windows_args \
XPILOT_COMMIT_ID=$test_commit \
BUILD_PACKAGE_SCRIPT="$fixture_tools/build-package-stub" \
BUILD_WINDOWS_PACKAGE_SCRIPT="$fixture_tools/build-windows-stub" \
    "$package_all_script" --version 4.7.99 --debian-revision 3 \
    --distro ubuntu --release 24.04 --arch amd64 --jobs 2 --debug

assert_equal "--target
deb
--version
4.7.99
--debian-revision
3
--distro
ubuntu
--release
24.04
--arch
amd64
--jobs
2
--debug" "$(cat "$all_args")" \
    "the complete package wrapper did not forward its arguments"
assert_equal "--target
windows
--arch
all
--package-version
4.7.99
--jobs
2
--build-type
Debug" "$(cat "$windows_args")" \
    "the complete package wrapper did not build every Windows package"
assert_equal "$test_commit" "$(cat "$all_args.commit")" \
    "the Debian package build did not receive the commit ID"
assert_equal "$test_commit" "$(cat "$windows_args.commit")" \
    "the Windows package build did not receive the commit ID"

prereq_project="$test_root/prereq-project"
prereq_log="$test_root/prereq.log"
mkdir -p "$prereq_project"
ln -s "$package_script" "$prereq_project/build_package.sh"
ln -s "$package_script_dir/tests" "$prereq_project/tests"

cat > "$fixture_tools/prereq-container-engine" <<'EOF'
#!/bin/sh
set -eu

if test "${1:-}" = image && test "${2:-}" = exists; then
    exit "${XPILOT_PREREQ_IMAGE_EXISTS:-1}"
fi
test "${1:-}" = build || exit 2

containerfile=
previous=
for argument do
    if test "$previous" = containerfile; then
        containerfile=$argument
        previous=
        continue
    fi
    test "$argument" = -f && previous=containerfile
done

printf '%s\n' "$*" > "$XPILOT_PREREQ_TEST_LOG"
cat "$containerfile" >> "$XPILOT_PREREQ_TEST_LOG"
EOF
chmod +x "$fixture_tools/prereq-container-engine"

XPILOT_PREREQ_TEST_LOG=$prereq_log \
XPILOT_PREREQ_PROJECT_ROOT=$prereq_project \
CONTAINER_ENGINE="$fixture_tools/prereq-container-engine" \
    "$prereq_script" --distro debian --release bookworm \
    --arch amd64 --jobs 1

assert_contains "$prereq_log" "--platform linux/amd64"
assert_contains "$prereq_log" \
    "localhost/xpilot-infinity-pack-deb-debian-bookworm-x86_64:latest"
assert_contains "$prereq_log" "dpkg-dev"
assert_contains "$prereq_log" "libalut-dev"
assert_contains "$prereq_log" "libfontconfig1-dev"
assert_contains "$prereq_log" "libgl-dev"
assert_contains "$prereq_log" "libopenal-dev"
assert_contains "$prereq_log" "libxrender-dev"
assert_contains "$prereq_log" "libxtst-dev"

# Existing image tags must not hide changes to the dependency recipe.
: > "$prereq_log"
XPILOT_PREREQ_IMAGE_EXISTS=0 \
XPILOT_PREREQ_TEST_LOG=$prereq_log \
XPILOT_PREREQ_PROJECT_ROOT=$prereq_project \
CONTAINER_ENGINE="$fixture_tools/prereq-container-engine" \
    "$prereq_script" --distro debian --release trixie \
    --arch arm64 --jobs 1
assert_contains "$prereq_log" "build "
assert_contains "$prereq_log" "--platform linux/arm64"
assert_not_contains "$prereq_log" "--no-cache"

XPILOT_PREREQ_IMAGE_EXISTS=0 \
XPILOT_PREREQ_TEST_LOG=$prereq_log \
XPILOT_PREREQ_PROJECT_ROOT=$prereq_project \
CONTAINER_ENGINE="$fixture_tools/prereq-container-engine" \
    "$prereq_script" --distro debian --release trixie \
    --arch arm64 --jobs 1 --force
assert_contains "$prereq_log" "--no-cache"

# Missing tray dependencies must give an actionable prerequisite diagnostic.
cat > "$fixture_tools/pkg-config" <<'EOF'
#!/bin/sh
exit "$XPILOT_TRAY_DEPS_STATUS"
EOF
chmod +x "$fixture_tools/pkg-config"
for deps_status in 1 0; do
    preflight_status=0
    if (
        BUILD_LINUX_DIST_SOURCE_ONLY=1
        export BUILD_LINUX_DIST_SOURCE_ONLY
        . "$linux_dist_script"
        XPILOT_TRAY_DEPS_STATUS=$deps_status
        export XPILOT_TRAY_DEPS_STATUS
        require_tray_dependencies
    ) > "$test_root/preflight.log" 2>&1; then
        :
    else
        preflight_status=$?
    fi
    if test "$deps_status" = 1; then
        test "$preflight_status" -ne 0 \
            || fail "missing tray dependencies were accepted"
        assert_contains "$test_root/preflight.log" "./prereq.sh"
    else
        assert_equal 0 "$preflight_status" "valid tray dependencies were rejected"
    fi
done

echo "Package build orchestration smoke passed"
