#!/bin/sh

set -eu

usage()
{
    cat <<'EOF'
Usage: package-windows.sh --source-dir PATH --build-dir PATH \
       --dependency-prefix PATH --output PATH

Assemble MinGW executables, vendored sound DLLs, and XPilot Infinity game data into
a relocatable Windows package directory.
EOF
}

fail()
{
    echo "package-windows.sh: $*" >&2
    exit 1
}

source_dir=
build_dir=
dependency_prefix=
package_dir=

while test "$#" -gt 0; do
    case "$1" in
        --source-dir)
            test "$#" -ge 2 || fail "--source-dir requires a path"
            source_dir=$2
            shift 2
            ;;
        --source-dir=*)
            source_dir=${1#*=}
            shift
            ;;
        --build-dir)
            test "$#" -ge 2 || fail "--build-dir requires a path"
            build_dir=$2
            shift 2
            ;;
        --build-dir=*)
            build_dir=${1#*=}
            shift
            ;;
        --dependency-prefix)
            test "$#" -ge 2 || fail "--dependency-prefix requires a path"
            dependency_prefix=$2
            shift 2
            ;;
        --dependency-prefix=*)
            dependency_prefix=${1#*=}
            shift
            ;;
        --output)
            test "$#" -ge 2 || fail "--output requires a path"
            package_dir=$2
            shift 2
            ;;
        --output=*)
            package_dir=${1#*=}
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

for required_path in \
    "$source_dir" "$build_dir" "$dependency_prefix" "$package_dir"; do
    test -n "$required_path" || fail "all path options are required"
    case "$required_path" in
        /*) ;;
        *) fail "all paths must be absolute" ;;
    esac
done
test -d "$source_dir" || fail "source directory was not found: $source_dir"
test -d "$build_dir" || fail "build directory was not found: $build_dir"
test -d "$dependency_prefix" \
    || fail "dependency prefix was not found: $dependency_prefix"
case "$package_dir" in
    "$build_dir"/*) ;;
    *) fail "package output must be inside the build directory" ;;
esac

server_executable="$build_dir/src/server/xpilot-infinity-server.exe"
client_executable="$build_dir/src/client/sdl/xpilot-infinity-sdl.exe"
tray_executable="$build_dir/src/tray/xpilot-infinity-tray.exe"
helper_executable="$build_dir/src/tray/xpilot-infinity-service-helper.exe"
test -f "$server_executable" \
    || fail "Windows server executable is missing: $server_executable"
test -f "$client_executable" \
    || fail "Windows SDL client executable is missing: $client_executable"
test -f "$tray_executable" || fail "Windows tray executable is missing: $tray_executable"
test -f "$helper_executable" || fail "Windows service helper is missing: $helper_executable"
openal_runtime="$dependency_prefix/bin/OpenAL32.dll"
test -f "$openal_runtime" \
    || fail "OpenAL Soft runtime is missing: $openal_runtime"
alut_runtime=
for alut_runtime_name in alut.dll libalut.dll freealut.dll; do
    if test -f "$dependency_prefix/bin/$alut_runtime_name"; then
        alut_runtime="$dependency_prefix/bin/$alut_runtime_name"
        break
    fi
done
test -n "$alut_runtime" \
    || fail "freealut runtime is missing from $dependency_prefix/bin"

package_work_dir="$package_dir.tmp"
for output_dir in "$package_dir" "$package_work_dir"; do
    case "$output_dir" in
        "$build_dir"/*) rm -rf -- "$output_dir" ;;
        *) fail "refusing to replace unexpected package path" ;;
    esac
done
mkdir -p "$package_work_dir/lib"
cp "$server_executable" "$client_executable" "$package_work_dir/"
cp "$tray_executable" "$helper_executable" "$package_work_dir/"
cp "$openal_runtime" "$alut_runtime" "$package_work_dir/"

for data_directory in fonts maps textures sound; do
    if test -d "$source_dir/lib/$data_directory"; then
        cp -R "$source_dir/lib/$data_directory" \
            "$package_work_dir/lib/$data_directory"
    fi
done
for data_file in defaults.txt password.txt robots.txt shipshapes.txt; do
    test -f "$source_dir/lib/$data_file" \
        || fail "required game data is missing: lib/$data_file"
    cp "$source_dir/lib/$data_file" "$package_work_dir/lib/"
done
cp "$source_dir/COPYING" "$package_work_dir/"
cp "$source_dir/README.md" "$source_dir/README_ja.md" "$package_work_dir/"
mkdir -p "$package_work_dir/licenses"
cp "$source_dir/vendor/openal-soft/COPYING" \
    "$package_work_dir/licenses/OpenAL-Soft-COPYING"
cp "$source_dir/vendor/freealut/COPYING" \
    "$package_work_dir/licenses/freealut-COPYING"

mv "$package_work_dir" "$package_dir"
echo "Windows package assembled in $package_dir"
