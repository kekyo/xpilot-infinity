#!/bin/sh

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
package_script=${BUILD_PACKAGE_SCRIPT:-$project_root/build_package.sh}
windows_package_script=${BUILD_WINDOWS_PACKAGE_SCRIPT:-$project_root/build.sh}
npm_package_script=${BUILD_NPM_PACKAGE_SCRIPT:-$project_root/build_package_npm.sh}
metadata_resolver=${XPILOT_BUILD_METADATA_RESOLVER:-$project_root/config/resolve-build-metadata.sh}

print_usage()
{
    cat <<'EOF'
Usage: ./build_package_all.sh [OPTIONS]

Build the Node.js npm package and shared browser assets, then the complete
Debian/Ubuntu matrix and Windows x86 and x86_64 ZIP archives and NSIS installers.
The npm build requires Node.js 24.x (24.19.0+) or 26.4.0+.
It uses emsdk-env for Emscripten.

Options:
  --version VERSION  Product version override (default: screw-up-derived version)
  --debian-revision REVISION
                     Debian packaging revision (default: 1)
  --distro LIST      Comma-separated Debian/Ubuntu distribution filter
  --release LIST     Comma-separated Debian/Ubuntu release filter
  --arch LIST        Comma-separated Linux architecture filter
  --jobs NUMBER      Concurrent package jobs and Windows make jobs
  --debug            Build every package without release optimization
  --print-version    Print the resolved product version and exit
  --help             Show this help

The distro, release, and architecture filters affect only Debian packages.
Both Windows architectures are always generated.
EOF
}

fail()
{
    echo "build_package_all.sh: $*" >&2
    exit 1
}

inspect_options()
{
    while test "$#" -gt 0; do
        case $1 in
            --version)
                test "$#" -ge 2 || fail "--version requires a value"
                windows_package_version=$2
                shift 2
                ;;
            --jobs)
                test "$#" -ge 2 || fail "--jobs requires a value"
                windows_jobs=$2
                shift 2
                ;;
            --debug)
                windows_build_type=Debug
                shift
                ;;
            --print-version)
                package_only=true
                shift
                ;;
            --help)
                print_usage
                exit 0
                ;;
            --debian-revision|--distro|--release|--arch)
                test "$#" -ge 2 || fail "$1 requires a value"
                shift 2
                ;;
            --target)
                fail "--target is not supported; this command always builds all package types"
                ;;
            *)
                fail "unknown argument: $1"
                ;;
        esac
    done
}

windows_package_version=
windows_jobs=
windows_build_type=Release
package_only=false

inspect_options "$@"

test -x "$metadata_resolver" \
    || fail "build metadata resolver is unavailable: $metadata_resolver"
resolved_metadata=$(XPILOT_VERSION=$windows_package_version \
    XPILOT_COMMIT_ID=${XPILOT_COMMIT_ID:-} "$metadata_resolver")
resolved_version=$(printf '%s\n' "$resolved_metadata" | sed -n '1p')
resolved_commit=$(printf '%s\n' "$resolved_metadata" | sed -n '2p')

if test "$package_only" = true; then
    printf '%s\n' "$resolved_version"
    exit 0
fi

if test -z "$windows_package_version"; then
    set -- "$@" --version "$resolved_version"
fi
windows_package_version=$resolved_version
XPILOT_COMMIT_ID=$resolved_commit
XPILOT_VERSION=$resolved_version
export XPILOT_COMMIT_ID XPILOT_VERSION

printf '%s\n' "===== build: npm package and shared browser assets ====="
"$npm_package_script"

printf '%s\n' "===== build: Debian and Ubuntu packages ====="
"$package_script" --target deb "$@"

set -- "$windows_package_script" --target windows --arch all
set -- "$@" --package-version "$windows_package_version"
if test -n "$windows_jobs"; then
    set -- "$@" --jobs "$windows_jobs"
fi
if test "$windows_build_type" = Debug; then
    set -- "$@" --build-type Debug
fi

printf '%s\n' "===== build: Windows x86 and x86_64 packages ====="
"$@"
