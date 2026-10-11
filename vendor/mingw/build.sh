#!/bin/sh

set -eu

usage()
{
    cat <<'EOF'
Usage: vendor/mingw/build.sh --build-dir PATH --prefix PATH [OPTIONS]

Cross-build the pinned zlib, Expat, OpenAL Soft, freealut, SDL3, SDL3_image,
and SDL3_ttf dependencies for a MinGW target and install them into one prefix.

Options:
  --build-dir PATH          Out-of-tree dependency build directory (required)
  --prefix PATH             Private installation prefix (required)
  --triplet TRIPLET         i686-w64-mingw32 or x86_64-w64-mingw32 (required)
  --toolchain-file PATH     Matching CMake toolchain file (required)
  --jobs NUMBER             Parallel build jobs (default: detected CPU count)
  --build-type TYPE         CMake build type (default: Release)
  --help                    Show this help

The build is performed on Linux and never downloads dependencies.  Initialize
all git submodules before invoking this script.
EOF
}

fail()
{
    echo "vendor/mingw/build.sh: $*" >&2
    exit 1
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
vendor_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
build_dir=
install_prefix=
triplet=
toolchain_file=
jobs=
build_type=Release

while test "$#" -gt 0; do
    case "$1" in
        --build-dir)
            test "$#" -ge 2 || fail "--build-dir requires a path"
            build_dir=$2
            shift 2
            ;;
        --build-dir=*)
            build_dir=${1#*=}
            shift
            ;;
        --prefix)
            test "$#" -ge 2 || fail "--prefix requires a path"
            install_prefix=$2
            shift 2
            ;;
        --prefix=*)
            install_prefix=${1#*=}
            shift
            ;;
        --triplet)
            test "$#" -ge 2 || fail "--triplet requires a value"
            triplet=$2
            shift 2
            ;;
        --triplet=*)
            triplet=${1#*=}
            shift
            ;;
        --toolchain-file)
            test "$#" -ge 2 || fail "--toolchain-file requires a path"
            toolchain_file=$2
            shift 2
            ;;
        --toolchain-file=*)
            toolchain_file=${1#*=}
            shift
            ;;
        --jobs)
            test "$#" -ge 2 || fail "--jobs requires a number"
            jobs=$2
            shift 2
            ;;
        --jobs=*)
            jobs=${1#*=}
            shift
            ;;
        --build-type)
            test "$#" -ge 2 || fail "--build-type requires a value"
            build_type=$2
            shift 2
            ;;
        --build-type=*)
            build_type=${1#*=}
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

test -n "$build_dir" || fail "--build-dir is required"
test -n "$install_prefix" || fail "--prefix is required"
test -n "$triplet" || fail "--triplet is required"
test -n "$toolchain_file" || fail "--toolchain-file is required"

case "$build_dir" in
    /*) ;;
    *) fail "--build-dir must be an absolute path" ;;
esac
case "$install_prefix" in
    /*) ;;
    *) fail "--prefix must be an absolute path" ;;
esac
case "$toolchain_file" in
    /*) ;;
    *) fail "--toolchain-file must be an absolute path" ;;
esac
test "$build_dir" != / || fail "refusing to use / as the build directory"
test "$install_prefix" != / || fail "refusing to install into /"
test "$build_dir" != "$install_prefix" \
    || fail "build directory and installation prefix must differ"
test -f "$toolchain_file" \
    || fail "toolchain file not found: $toolchain_file"

case "$triplet" in
    i686-w64-mingw32|x86_64-w64-mingw32) ;;
    *) fail "unsupported MinGW triplet: $triplet" ;;
esac

if test -z "$jobs"; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
fi
case "$jobs" in
    ''|0|*[!0-9]*) fail "--jobs must be a positive integer" ;;
esac
test -n "$build_type" || fail "--build-type requires a nonempty value"

# This dependency pipeline owns --jobs. Its sequential zlib/CMake builds each
# receive that limit, independently of the enclosing Autotools jobserver.
unset MAKEFLAGS MFLAGS

zlib_source="$vendor_dir/zlib"
expat_source="$vendor_dir/expat/expat"
openal_source="$vendor_dir/openal-soft"
freealut_source="$vendor_dir/freealut"
sdl_builder="$vendor_dir/sdl3/build.sh"
test -x "$zlib_source/configure" \
    || fail "missing zlib submodule; run git submodule update --init"
test -f "$expat_source/CMakeLists.txt" \
    || fail "missing Expat submodule; run git submodule update --init"
test -f "$openal_source/CMakeLists.txt" \
    || fail "missing OpenAL Soft submodule; run git submodule update --init"
test -f "$freealut_source/CMakeLists.txt" \
    || fail "missing freealut submodule; run git submodule update --init"
test -x "$sdl_builder" \
    || fail "vendored SDL3 builder is unavailable: $sdl_builder"

make_program=${MAKE:-make}
for required_command in cmake cp "$make_program" \
    "$triplet-gcc" "$triplet-g++"; do
    command -v "$required_command" >/dev/null 2>&1 \
        || fail "required command was not found: $required_command"
done

mkdir -p "$build_dir" "$install_prefix"
build_dir=$(CDPATH= cd -- "$build_dir" && pwd)
install_prefix=$(CDPATH= cd -- "$install_prefix" && pwd)

zlib_work_dir="$build_dir/zlib-source"
case "$zlib_work_dir" in
    "$build_dir"/*) rm -rf -- "$zlib_work_dir" ;;
    *) fail "refusing to reset unexpected zlib build path" ;;
esac
mkdir -p "$zlib_work_dir"
cp -a "$zlib_source/." "$zlib_work_dir/"

echo "===== build: zlib for $triplet ====="
(
    cd "$zlib_work_dir"
    "$make_program" -f win32/Makefile.gcc "-j$jobs" \
        "PREFIX=$triplet-" libz.a
    mkdir -p "$install_prefix/include" "$install_prefix/lib"
    cp zlib.h zconf.h "$install_prefix/include/"
    cp libz.a "$install_prefix/lib/"
)

expat_build_dir="$build_dir/expat"
echo "===== build: Expat for $triplet ====="
cmake -S "$expat_source" -B "$expat_build_dir" \
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain_file" \
    "-DCMAKE_BUILD_TYPE=$build_type" \
    "-DCMAKE_INSTALL_PREFIX=$install_prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DEXPAT_SHARED_LIBS=OFF \
    -DEXPAT_BUILD_DOCS=OFF \
    -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TESTS=OFF \
    -DEXPAT_BUILD_TOOLS=OFF \
    -DEXPAT_BUILD_PKGCONFIG=ON
cmake --build "$expat_build_dir" --config "$build_type" \
    --parallel "$jobs"
cmake --install "$expat_build_dir" --config "$build_type"

openal_build_dir="$build_dir/openal-soft"
echo "===== build: OpenAL Soft for $triplet ====="
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR="$install_prefix/lib/pkgconfig" \
cmake -S "$openal_source" -B "$openal_build_dir" \
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain_file" \
    "-DCMAKE_BUILD_TYPE=$build_type" \
    "-DCMAKE_INSTALL_PREFIX=$install_prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DLIBTYPE=SHARED \
    -DALSOFT_UTILS=OFF \
    -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_TESTS=OFF \
    -DALSOFT_INSTALL_CONFIG=OFF \
    -DALSOFT_INSTALL_HRTF_DATA=OFF \
    -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF \
    -DALSOFT_INSTALL_EXAMPLES=OFF \
    -DALSOFT_INSTALL_UTILS=OFF \
    -DALSOFT_UPDATE_BUILD_VERSION=OFF \
    -DALSOFT_STATIC_LIBGCC=ON \
    -DALSOFT_STATIC_STDCXX=ON \
    -DALSOFT_STATIC_WINPTHREAD=ON \
    -DALSOFT_BACKEND_PIPEWIRE=OFF \
    -DALSOFT_BACKEND_PULSEAUDIO=OFF \
    -DALSOFT_BACKEND_JACK=OFF \
    -DALSOFT_BACKEND_COREAUDIO=OFF \
    -DALSOFT_BACKEND_OBOE=OFF \
    -DALSOFT_BACKEND_OPENSL=OFF \
    -DALSOFT_BACKEND_PORTAUDIO=OFF \
    -DALSOFT_BACKEND_SDL3=OFF \
    -DALSOFT_BACKEND_SDL2=OFF
cmake --build "$openal_build_dir" --config "$build_type" \
    --parallel "$jobs"
cmake --install "$openal_build_dir" --config "$build_type"

openal_import_library=
for candidate_path in \
    "$install_prefix/lib/libOpenAL32.dll.a" \
    "$install_prefix/lib/OpenAL32.lib"; do
    if test -f "$candidate_path"; then
        openal_import_library=$candidate_path
        break
    fi
done
test -n "$openal_import_library" \
    || fail "OpenAL Soft import library was not installed"

freealut_build_dir="$build_dir/freealut"
echo "===== build: freealut for $triplet ====="
set --
# CMake 4 provides a public override for a dependency's old policy baseline.
# Older CMake versions do not recognize this option.
if cmake --help-variable CMAKE_POLICY_VERSION_MINIMUM >/dev/null 2>&1; then
    set -- -DCMAKE_POLICY_VERSION_MINIMUM=3.10
fi
cmake -S "$freealut_source" -B "$freealut_build_dir" \
    "$@" \
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain_file" \
    "-DCMAKE_BUILD_TYPE=$build_type" \
    "-DCMAKE_INSTALL_PREFIX=$install_prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    "-DOPENAL_INCLUDE_DIR=$install_prefix/include" \
    "-DOPENAL_LIBRARY=$openal_import_library" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF
cmake --build "$freealut_build_dir" --config "$build_type" \
    --parallel "$jobs"
cmake --install "$freealut_build_dir" --config "$build_type"

echo "===== build: SDL3 dependencies for $triplet ====="
"$sdl_builder" \
    --build-dir "$build_dir/sdl3" \
    --prefix "$install_prefix" \
    --jobs "$jobs" \
    --build-type "$build_type" \
    --toolchain-file "$toolchain_file" \
    --vendored-font-deps \
    --disable-libusb

for required_file in \
    "$install_prefix/lib/libz.a" \
    "$install_prefix/lib/libexpat.a" \
    "$install_prefix/bin/OpenAL32.dll" \
    "$install_prefix/lib/pkgconfig/openal.pc" \
    "$install_prefix/lib/pkgconfig/freealut.pc" \
    "$install_prefix/lib/pkgconfig/sdl3.pc" \
    "$install_prefix/lib/pkgconfig/sdl3-image.pc" \
    "$install_prefix/lib/pkgconfig/sdl3-ttf.pc"; do
    test -f "$required_file" \
        || fail "expected target dependency was not installed: $required_file"
done

alut_runtime_found=false
for alut_runtime_name in alut.dll libalut.dll freealut.dll; do
    if test -f "$install_prefix/bin/$alut_runtime_name"; then
        alut_runtime_found=true
        break
    fi
done
test "$alut_runtime_found" = true \
    || fail "freealut runtime DLL was not installed"

echo "MinGW dependencies for $triplet installed in $install_prefix"
