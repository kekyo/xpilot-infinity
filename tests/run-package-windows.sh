#!/bin/sh

set -eu

fail()
{
    echo "run-package-windows.sh: $*" >&2
    exit 1
}

test -n "${XPILOT_WINDOWS_PACKAGER:-}" \
    || fail "XPILOT_WINDOWS_PACKAGER is not set"
test -x "$XPILOT_WINDOWS_PACKAGER" \
    || fail "Windows packager is unavailable: $XPILOT_WINDOWS_PACKAGER"

test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-windows-package-test.XXXXXX")

cleanup()
{
    case "$test_root" in
        "${TMPDIR:-/tmp}"/xpilot-windows-package-test.*)
            rm -rf -- "$test_root"
            ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

source_dir="$test_root/source"
build_dir="$test_root/build"
dependency_prefix="$test_root/dependencies"
package_dir="$build_dir/package"

mkdir -p "$source_dir/lib/maps" "$source_dir/lib/sound" \
    "$source_dir/vendor/openal-soft" "$source_dir/vendor/freealut" \
    "$build_dir/src/server" "$build_dir/src/client/sdl" "$build_dir/src/tray" \
    "$dependency_prefix/bin"

for data_file in defaults.txt password.txt robots.txt shipshapes.txt; do
    printf 'fixture data\n' > "$source_dir/lib/$data_file"
done
printf 'fixture map\n' > "$source_dir/lib/maps/ndh.xp2"
printf 'fixture sound map\n' > "$source_dir/lib/sound/sounds.txt"
printf 'fixture license\n' > "$source_dir/COPYING"
printf 'fixture OpenAL license\n' > "$source_dir/vendor/openal-soft/COPYING"
printf 'fixture freealut license\n' > "$source_dir/vendor/freealut/COPYING"
printf 'fixture server\n' > "$build_dir/src/server/xpilot-infinity-server.exe"
printf 'fixture client\n' > "$build_dir/src/client/sdl/xpilot-infinity-sdl.exe"
printf 'fixture tray\n' > "$build_dir/src/tray/xpilot-infinity-tray.exe"
printf 'fixture helper\n' > "$build_dir/src/tray/xpilot-infinity-service-helper.exe"
printf 'fixture usage\n' > "$source_dir/README.md"
printf 'fixture usage ja\n' > "$source_dir/README_ja.md"
printf 'fixture OpenAL\n' > "$dependency_prefix/bin/OpenAL32.dll"
printf 'fixture freealut\n' > "$dependency_prefix/bin/alut.dll"

"$XPILOT_WINDOWS_PACKAGER" \
    --source-dir "$source_dir" \
    --build-dir "$build_dir" \
    --dependency-prefix "$dependency_prefix" \
    --output "$package_dir"

for expected_file in \
    "$package_dir/xpilot-infinity-server.exe" \
    "$package_dir/xpilot-infinity-sdl.exe" \
    "$package_dir/xpilot-infinity-tray.exe" \
    "$package_dir/xpilot-infinity-service-helper.exe" \
    "$package_dir/README.md" "$package_dir/README_ja.md" \
    "$package_dir/OpenAL32.dll" \
    "$package_dir/alut.dll" \
    "$package_dir/licenses/OpenAL-Soft-COPYING" \
    "$package_dir/licenses/freealut-COPYING" \
    "$package_dir/lib/sound/sounds.txt"
do
    test -f "$expected_file" \
        || fail "Windows package output is missing: $expected_file"
done

grep -Fq 'fixture OpenAL' "$package_dir/OpenAL32.dll" \
    || fail "the packaged OpenAL DLL did not come from the dependency prefix"
grep -Fq 'fixture freealut' "$package_dir/alut.dll" \
    || fail "the packaged freealut DLL did not come from the dependency prefix"

echo "Windows sound dependency packaging passed"
