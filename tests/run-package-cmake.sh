#!/bin/sh

set -eu

test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-package-cmake.XXXXXX")
trap 'rm -rf -- "$test_root"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

# Exercise discovery, linking and execution: pkg-config metadata alone does
# not prove that CMake can locate the installed font libraries.
cat > "$test_root/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.18)
project(package_font_probe C)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz>=2.3.1)
find_library(HARFBUZZ_LIBRARY NAMES harfbuzz
    HINTS ${HARFBUZZ_LIBRARY_DIRS} REQUIRED)
find_package(Freetype REQUIRED)
add_executable(font-probe main.c)
set_property(TARGET font-probe PROPERTY C_STANDARD 99)
target_include_directories(font-probe PRIVATE ${HARFBUZZ_INCLUDE_DIRS})
target_link_libraries(font-probe PRIVATE
    "${HARFBUZZ_LIBRARY}" Freetype::Freetype)
EOF
cat > "$test_root/main.c" <<'EOF'
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

int main(void)
{
    if (!hb_version_atleast(2, 3, 1))
        return 1;
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
        return 1;
    return FT_Done_FreeType(library) != 0;
}
EOF

cmake -S "$test_root" -B "$test_root/build"
cmake --build "$test_root/build"
"$test_root/build/font-probe"
echo 'Package CMake font dependency discovery, linking and execution passed'
