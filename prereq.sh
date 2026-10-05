#!/bin/sh

set -eu

PROJECT_ROOT=${XPILOT_PREREQ_PROJECT_ROOT:-${BUILD_PACKAGE_PROJECT_ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}}
BUILD_PACKAGE_PROJECT_ROOT=$PROJECT_ROOT
BUILD_PACKAGE_SOURCE_ONLY=1
export BUILD_PACKAGE_PROJECT_ROOT BUILD_PACKAGE_SOURCE_ONLY
. "$PROJECT_ROOT/build_package.sh"
unset BUILD_PACKAGE_SOURCE_ONLY BUILD_PACKAGE_PROJECT_ROOT

PACKAGE_BUILD_ROOT="$PROJECT_ROOT/.build/package"
DEFAULT_PREREQ_PARALLEL_JOB_CAP=4

print_usage()
{
    cat <<'EOF'
Usage: ./prereq.sh [OPTIONS]

Build the container images used by build_package.sh.

Options:
  --distro LIST   Comma-separated distribution filter
  --release LIST  Comma-separated release filter
  --arch LIST     Comma-separated architecture filter
  --jobs NUMBER   Concurrent image builds (default: auto, up to 4)
  --force         Rebuild without using cached layers
  --help          Show this help
EOF
}

container_packages()
{
    cat <<'EOF'
autoconf
automake
binutils
build-essential
ca-certificates
cmake
dpkg-dev
file
libalut-dev
libbrotli-dev
libbz2-dev
libexpat1-dev
libfontconfig1-dev
libfreetype-dev
libgl-dev
libglib2.0-dev
libgtk-3-dev
libpolkit-gobject-1-dev
libgraphite2-dev
libharfbuzz-dev
libice-dev
libopenal-dev
libpng-dev
libsm-dev
libx11-dev
libxcursor-dev
libxext-dev
libxfixes-dev
libxi-dev
libxrandr-dev
libxrender-dev
libxss-dev
libxtst-dev
pkg-config
nodejs
npm
zlib1g-dev
EOF
}

write_containerfile()
{
    containerfile=$1
    {
        cat <<'EOF'
ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
EOF
        container_packages | while IFS= read -r package_name; do
            test -n "$package_name" || continue
            printf '      %s \\\n' "$package_name"
        done
        cat <<'EOF'
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 22.04 ships Node 12; the build-time template CLI requires Node 16+.
# Use distro Node on the other targets, including Debian's 32-bit builds.
# Upgrade npm together with Node so it carries its own dependencies instead
# of relying on the distro Node's shared module lookup paths.
RUN node -e 'process.exit(Number(process.versions.node.split(".")[0]) >= 16 ? 0 : 1)' \
    || (npm install --global node@22 && \
        NODE_PATH=/usr/share/nodejs /usr/local/bin/node /usr/bin/npm install --global npm@10)

COPY run-package-tools.sh /tmp/run-package-tools.sh
RUN sh /tmp/run-package-tools.sh && rm /tmp/run-package-tools.sh
EOF
    } > "$containerfile"
}

build_prereq_image()
{
    distro=$1
    release=$2
    arch=$3
    platform=$4
    base_image=$(container_image_for_target "$distro" "$release" "$arch")
    prereq_image=$(prereq_image_for_target "$distro" "$release" "$arch")
    work_dir="$TMP_ROOT/deb/$distro/$release/$arch"
    containerfile="$work_dir/Containerfile"

    printf '%s\n' \
        "[prereq:deb] build $prereq_image ($platform, $base_image)"
    rm -rf "$work_dir"
    mkdir -p "$work_dir"
    write_containerfile "$containerfile"
    cp "$PROJECT_ROOT/tests/run-package-tools.sh" "$work_dir/"

    if test "$FORCE" -eq 1; then
        set -- --no-cache
    else
        set --
    fi
    # Let the build cache reuse unchanged layers, not an outdated image tag.
    "$CONTAINER_ENGINE_BIN" build "$@" \
        --platform "$platform" \
        --pull=missing \
        --build-arg "BASE_IMAGE=$base_image" \
        -t "$prereq_image" \
        -f "$containerfile" \
        "$work_dir"
}

wait_for_oldest_prereq_job()
{
    test "$ACTIVE_JOB_COUNT" -gt 0 || return 0
    set -- $ACTIVE_JOB_PIDS
    wait_pid=$1
    shift
    if wait "$wait_pid"; then
        :
    else
        JOB_FAILURE=1
    fi
    ACTIVE_JOB_PIDS=$*
    ACTIVE_JOB_COUNT=$((ACTIVE_JOB_COUNT - 1))
}

run_parallel_prereq_job()
{
    while test "$ACTIVE_JOB_COUNT" -ge "$PARALLEL_JOBS"; do
        wait_for_oldest_prereq_job
    done
    test "$JOB_FAILURE" -eq 0 \
        || fail "one or more prerequisite image builds failed"
    "$@" &
    ACTIVE_JOB_PIDS="${ACTIVE_JOB_PIDS}${ACTIVE_JOB_PIDS:+ }$!"
    ACTIVE_JOB_COUNT=$((ACTIVE_JOB_COUNT + 1))
}

wait_for_all_prereq_jobs()
{
    while test "$ACTIVE_JOB_COUNT" -gt 0; do
        wait_for_oldest_prereq_job
    done
    test "$JOB_FAILURE" -eq 0 \
        || fail "one or more prerequisite image builds failed"
}

schedule_image_builds()
{
    while IFS=' ' read -r distro release arch platform; do
        test -n "$distro" || continue
        matches_filter "$DISTRO_FILTER" "$distro" || continue
        matches_filter "$RELEASE_FILTER" "$release" || continue
        matches_filter "$ARCH_FILTER" "$arch" || continue
        SCHEDULED_TASK_COUNT=$((SCHEDULED_TASK_COUNT + 1))
        run_parallel_prereq_job \
            build_prereq_image "$distro" "$release" "$arch" "$platform"
    done <<EOF
$LINUX_MATRIX
EOF
}

main()
{
    DISTRO_FILTER=
    RELEASE_FILTER=
    ARCH_FILTER=
    PARALLEL_JOBS=
    FORCE=0

    while test "$#" -gt 0; do
        case $1 in
            --distro)
                test "$#" -ge 2 || fail "--distro requires a value"
                DISTRO_FILTER=$(normalize_filter_list distro "$2")
                shift 2
                ;;
            --release)
                test "$#" -ge 2 || fail "--release requires a value"
                RELEASE_FILTER=$(normalize_filter_list release "$2")
                shift 2
                ;;
            --arch)
                test "$#" -ge 2 || fail "--arch requires a value"
                ARCH_FILTER=$(normalize_filter_list arch "$2")
                shift 2
                ;;
            --jobs)
                test "$#" -ge 2 || fail "--jobs requires a value"
                PARALLEL_JOBS=$2
                shift 2
                ;;
            --force)
                FORCE=1
                shift
                ;;
            --help)
                print_usage
                exit 0
                ;;
            *) fail "unknown argument: $1" ;;
        esac
    done

    if test -n "$PARALLEL_JOBS"; then
        validate_positive_integer "parallel job count" "$PARALLEL_JOBS"
    else
        PARALLEL_JOBS=$(min_int \
            "$(detect_processor_count)" "$DEFAULT_PREREQ_PARALLEL_JOB_CAP")
    fi

    CONTAINER_ENGINE_BIN=$(choose_container_engine)
    RUN_ID="prereq-$(date +%Y%m%d%H%M%S)-$$"
    TMP_ROOT="$PACKAGE_BUILD_ROOT/tmp/$RUN_ID"
    ACTIVE_JOB_PIDS=
    ACTIVE_JOB_COUNT=0
    JOB_FAILURE=0
    SCHEDULED_TASK_COUNT=0

    mkdir -p "$TMP_ROOT"
    printf '%s\n' "Using up to $PARALLEL_JOBS prerequisite image jobs"
    schedule_image_builds
    test "$SCHEDULED_TASK_COUNT" -gt 0 \
        || fail "no prerequisite image targets matched"
    wait_for_all_prereq_jobs
    rm -rf "$TMP_ROOT"
    printf '%s\n' "Prerequisite images are ready"
}

main "$@"
