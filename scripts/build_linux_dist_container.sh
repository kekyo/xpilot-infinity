#!/bin/sh

set -eu

fail()
{
    echo "build_linux_dist_container.sh: $*" >&2
    exit 1
}

require_env()
{
    variable_name=$1
    eval "variable_value=\${$variable_name:-}"
    test -n "$variable_value" \
        || fail "required environment variable is missing: $variable_name"
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 \
        || fail "required command was not found: $1"
}

assert_file()
{
    test -f "$1" || fail "missing expected file: $1"
}

assert_debian_dependency()
{
    dependency_list=$1
    expected_package=$2
    saved_ifs=$IFS
    IFS=,
    for dependency in $dependency_list; do
        dependency=${dependency# }
        case $dependency in
            "$expected_package"|"$expected_package "*)
                IFS=$saved_ifs
                return 0
                ;;
        esac
    done
    IFS=$saved_ifs
    fail "missing runtime dependency: $expected_package"
}

validate_positive_integer()
{
    case $2 in
        ''|*[!0-9]*) fail "$1 must be a positive integer: $2" ;;
    esac
    test "$2" -gt 0 || fail "$1 must be a positive integer: $2"
}

validate_copyright_file()
{
    copyright_path=$1
    assert_file "$copyright_path"
    if grep -F TODO "$copyright_path" >/dev/null 2>&1; then
        fail "copyright metadata still contains TODO entries: $copyright_path"
    fi
}

validate_debian_changelog()
{
    changelog_path=$1
    assert_file "$changelog_path"
    changelog_version=$(dpkg-parsechangelog \
        -l "$changelog_path" -S Version)
    test "$changelog_version" = "$XPILOT_PACKAGE_VERSION" \
        || fail "Debian changelog version $changelog_version does not match package version $XPILOT_PACKAGE_VERSION"
}

render_debian_changelog()
{
    changelog_template_path=$1
    changelog_output_path=$2
    version_placeholder=@XPILOT_PACKAGE_VERSION@

    assert_file "$changelog_template_path"
    grep -F "$version_placeholder" "$changelog_template_path" \
        >/dev/null 2>&1 \
        || fail "Debian changelog template has no version placeholder: $changelog_template_path"
    sed "s/$version_placeholder/$XPILOT_PACKAGE_VERSION/g" \
        "$changelog_template_path" > "$changelog_output_path"
    if grep -F "$version_placeholder" "$changelog_output_path" \
        >/dev/null 2>&1
    then
        fail "Debian changelog still contains a version placeholder: $changelog_output_path"
    fi
    validate_debian_changelog "$changelog_output_path"
}

strip_staged_executables()
{
    strip_command=${STRIP:-strip}
    for executable_name in \
        xpilot-infinity-sdl xpilot-infinity-x11 xpilot-infinity-server \
        xpilot-infinity-replay xpilot-infinity-xp-mapedit xpilot-infinity-tray
    do
        executable_path="$stage_dir/usr/games/$executable_name"
        assert_file "$executable_path"
        "$strip_command" --strip-unneeded \
            --remove-section=.comment --remove-section=.note \
            "$executable_path"
    done
    "$strip_command" --strip-unneeded --remove-section=.comment --remove-section=.note \
        "$stage_dir/usr/lib/xpilot-infinity/xpilot-infinity-settings-helper"
}

compress_manual_pages()
{
    manual_count=0
    for manual_path in "$stage_dir"/usr/share/man/man[1-9]/*.[1-9]; do
        test -f "$manual_path" || continue
        gzip -9n "$manual_path"
        manual_count=$((manual_count + 1))
    done
    test "$manual_count" -gt 0 \
        || fail "no manual pages were available to compress"
}

install_package_documentation()
{
    copyright_path="$source_dir/debian/copyright"
    debian_changelog_template_path="$source_dir/debian/changelog.in"
    debian_changelog_path="$meta_dir/changelog.Debian"
    validate_copyright_file "$copyright_path"

    package_doc_dir="$stage_dir/usr/share/doc/$XPILOT_PACKAGE_NAME"
    mkdir -p "$package_doc_dir" "$meta_dir"
    render_debian_changelog \
        "$debian_changelog_template_path" "$debian_changelog_path"
    cp "$source_dir/README.md" "$package_doc_dir/"
    cp "$source_dir/README_ja.md" "$package_doc_dir/"
    cp "$copyright_path" "$package_doc_dir/copyright"
    gzip -9n -c "$source_dir/ChangeLog" \
        > "$package_doc_dir/changelog.gz"
    gzip -9n -c "$debian_changelog_path" \
        > "$package_doc_dir/changelog.Debian.gz"
}

install_systemd_service()
{
    unit_name=xpilot-infinity-server.service
    defaults_name=xpilot-infinity-server
    unit_source="$source_dir/debian/$unit_name"
    defaults_source="$source_dir/debian/$defaults_name.default"
    assert_file "$unit_source"
    assert_file "$defaults_source"

    unit_dir="$stage_dir/usr/lib/systemd/system"
    defaults_dir="$stage_dir/etc/default"
    control_dir="$stage_dir/DEBIAN"
    mkdir -p "$unit_dir" "$defaults_dir" "$control_dir"
    install -m 0644 "$unit_source" "$unit_dir/$unit_name"
    install -m 0644 "$defaults_source" "$defaults_dir/$defaults_name"
    printf '/etc/default/%s\n' "$defaults_name" \
        > "$control_dir/conffiles"
    printf '%s\n' /etc/dbus-1/system.d/org.xpilot.Infinity.ServerSettings1.conf >> "$control_dir/conffiles"

    for maintainer_script in postinst prerm postrm; do
        maintainer_source="$source_dir/debian/$XPILOT_PACKAGE_NAME.$maintainer_script"
        assert_file "$maintainer_source"
        install -m 0755 "$maintainer_source" \
            "$control_dir/$maintainer_script"
    done
}

install_desktop_launcher()
{
    desktop_source="$source_dir/debian/xpilot-infinity.desktop"
    icon_source="$source_dir/images/icon-1254.png"
    assert_file "$desktop_source"
    assert_file "$icon_source"

    applications_dir="$stage_dir/usr/share/applications"
    pixmaps_dir="$stage_dir/usr/share/pixmaps"
    mkdir -p "$applications_dir" "$pixmaps_dir"
    install -m 0644 "$desktop_source" \
        "$applications_dir/xpilot-infinity.desktop"
    install -m 0644 "$icon_source" \
        "$pixmaps_dir/xpilot-infinity.png"
}

prepare_debian_package_files()
{
    strip_staged_executables
    compress_manual_pages
    install_package_documentation
    install_desktop_launcher
    install_systemd_service
}

calculate_runtime_dependencies()
{
    temporary_dir=$(mktemp -d)
    mkdir -p "$temporary_dir/debian"
    cat > "$temporary_dir/debian/control" <<EOF
Source: $XPILOT_PACKAGE_NAME
Section: games
Priority: optional
Maintainer: $XPILOT_PACKAGE_MAINTAINER
Standards-Version: 4.7.4

Package: $XPILOT_PACKAGE_NAME
Architecture: $deb_arch
Description: temporary metadata for dependency calculation
EOF

    dependencies=$(
        cd "$temporary_dir"
        dpkg-shlibdeps -O \
            "$stage_dir/usr/games/xpilot-infinity-sdl" \
            "$stage_dir/usr/games/xpilot-infinity-x11" \
            "$stage_dir/usr/games/xpilot-infinity-server" \
            "$stage_dir/usr/games/xpilot-infinity-replay" \
            "$stage_dir/usr/games/xpilot-infinity-xp-mapedit" \
            "$stage_dir/usr/games/xpilot-infinity-tray" \
            "$stage_dir/usr/lib/xpilot-infinity/xpilot-infinity-settings-helper" \
            | sed -n 's/^shlibs:Depends=//p'
    )
    rm -rf "$temporary_dir"
    test -n "$dependencies" \
        || fail "dpkg-shlibdeps did not calculate runtime dependencies"
    printf '%s\n' "$dependencies"
}

write_control_file()
{
    dependencies=$1
    control_dir="$stage_dir/DEBIAN"
    mkdir -p "$control_dir"
    cat > "$control_dir/control" <<EOF
Package: $XPILOT_PACKAGE_NAME
Version: $XPILOT_PACKAGE_VERSION
Section: games
Priority: optional
Architecture: $deb_arch
Maintainer: $XPILOT_PACKAGE_MAINTAINER
Pre-Depends: init-system-helpers (>= 1.54~)
Depends: $dependencies, dbus, polkitd | policykit-1
Description: $XPILOT_PACKAGE_DESCRIPTION
 XPilot Infinity is a multiplayer tactical game. This package includes the SDL
 and X11 clients, the dedicated server, service tray, utilities, game data, and manuals.
EOF
}

if test "${BUILD_LINUX_DIST_SOURCE_ONLY:-0}" = 1; then
    return 0 2>/dev/null || exit 0
fi

for variable_name in \
    XPILOT_WORK_DIR XPILOT_META_DIR \
    XPILOT_VERSION XPILOT_COMMIT_ID \
    XPILOT_PACKAGE_VERSION XPILOT_PACKAGE_NAME XPILOT_PACKAGE_DESCRIPTION \
    XPILOT_PACKAGE_MAINTAINER XPILOT_BUILD_TYPE XPILOT_MAKE_JOBS
do
    require_env "$variable_name"
done

validate_positive_integer XPILOT_MAKE_JOBS "$XPILOT_MAKE_JOBS"
case $XPILOT_WORK_DIR in
    /workspace/artifacts/.tmp/*/work) ;;
    *) fail "unsafe work directory: $XPILOT_WORK_DIR" ;;
esac
case $XPILOT_META_DIR in
    /workspace/artifacts/.tmp/*/meta) ;;
    *) fail "unsafe metadata directory: $XPILOT_META_DIR" ;;
esac

require_command cmake
require_command dpkg-architecture
require_command dpkg-parsechangelog
require_command dpkg-shlibdeps
require_command gzip
require_command make
require_command pkg-config
require_command "${STRIP:-strip}"

test -x ./configure || fail "configure is unavailable; run ./bootstrap first"
test -x ./vendor/sdl3/build.sh \
    || fail "the vendored SDL3 build script is unavailable"
test ! -f ./config.status \
    || fail "the source tree must not be configured in place"

work_dir=$XPILOT_WORK_DIR
meta_dir=$XPILOT_META_DIR
source_dir=$(pwd)
dependency_build_dir="$work_dir/vendor-sdl3"
dependency_prefix="$work_dir/vendor-sdl3-prefix"
build_dir="$work_dir/xpilot-infinity"
stage_dir="$work_dir/stage/$XPILOT_PACKAGE_NAME"

rm -rf "$work_dir" "$meta_dir"
mkdir -p "$build_dir" "$meta_dir" "$stage_dir"

./vendor/sdl3/build.sh \
    --build-dir "$dependency_build_dir" \
    --prefix "$dependency_prefix" \
    --jobs "$XPILOT_MAKE_JOBS" \
    --build-type "$XPILOT_BUILD_TYPE" \
    --disable-libusb

(
    cd "$build_dir"
    /workspace/configure \
        --prefix=/usr \
        --bindir=/usr/games \
        --datadir=/usr/share/games \
        --mandir=/usr/share/man \
        --sysconfdir=/etc \
        --libexecdir=/usr/lib/xpilot-infinity \
        --enable-server-tray=yes \
        --with-sdl3=vendored \
        "--with-sdl3-prefix=$dependency_prefix"
    make -j"$XPILOT_MAKE_JOBS" \
        "XPILOT_VERSION=$XPILOT_VERSION" \
        "XPILOT_COMMIT_ID=$XPILOT_COMMIT_ID"
    make "XPILOT_VERSION=$XPILOT_VERSION" \
        "XPILOT_COMMIT_ID=$XPILOT_COMMIT_ID" \
        install DESTDIR="$stage_dir"
)

for executable_name in \
    xpilot-infinity-sdl xpilot-infinity-x11 xpilot-infinity-server \
    xpilot-infinity-replay xpilot-infinity-xp-mapedit xpilot-infinity-tray
do
    assert_file "$stage_dir/usr/games/$executable_name"
done
assert_file "$stage_dir/usr/lib/xpilot-infinity/xpilot-infinity-settings-helper"
assert_file "$stage_dir/usr/share/applications/xpilot-infinity-tray.desktop"
assert_file "$stage_dir/usr/share/dbus-1/system-services/org.xpilot.Infinity.ServerSettings1.service"
assert_file "$stage_dir/usr/share/polkit-1/actions/org.xpilot.infinity.policy"
assert_file "$stage_dir/usr/share/games/xpilot-infinity/defaults.txt"
assert_file "$stage_dir/usr/share/games/xpilot-infinity/maps/ndh.xp2"
assert_file "$stage_dir/usr/share/games/xpilot-infinity/sound/sounds.txt"
assert_file "$stage_dir/usr/share/games/xpilot-infinity/sound/bfire.wav"
prepare_debian_package_files
assert_file "$stage_dir/usr/share/man/man6/xpilot-infinity-sdl.6.gz"
assert_file "$stage_dir/usr/share/doc/$XPILOT_PACKAGE_NAME/copyright"
assert_file "$stage_dir/usr/share/doc/$XPILOT_PACKAGE_NAME/changelog.gz"
assert_file \
    "$stage_dir/usr/share/doc/$XPILOT_PACKAGE_NAME/changelog.Debian.gz"
assert_file "$stage_dir/usr/share/applications/xpilot-infinity.desktop"
assert_file "$stage_dir/usr/share/pixmaps/xpilot-infinity.png"

deb_arch=$(dpkg-architecture -qDEB_HOST_ARCH)
runtime_dependencies=$(calculate_runtime_dependencies)
assert_debian_dependency "$runtime_dependencies" libalut0
assert_debian_dependency "$runtime_dependencies" libopenal1
write_control_file "$runtime_dependencies"

printf '%s\n' "$deb_arch" > "$meta_dir/deb_arch"
