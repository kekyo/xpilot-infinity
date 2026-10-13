#!/bin/sh
set -eu

tray_build_dir=$(CDPATH= cd .. && pwd)
tray_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
top_build_dir=$(CDPATH= cd "$tray_build_dir/../.." && pwd)
test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-tray-templates.XXXXXX")
trap 'rm -rf -- "$test_root"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

# Distribution npm versions may install both CLIs into the same cache directory
# without locking. Model an unavailable CLI during that shared installation.
cat > "$test_root/funcity" <<'EOF'
#!/bin/sh
set -eu
mkdir "$XPILOT_TEMPLATE_TEST_LOCK" 2>/dev/null \
    || { echo 'Template CLI installation overlaps another invocation' >&2; exit 1; }
trap 'rmdir "$XPILOT_TEMPLATE_TEST_LOCK"' EXIT
npm exec --yes --package=commander@11.1.0 --package=funcity-cli@1.5.0 -- funcity "$@"
EOF
chmod +x "$test_root/funcity"
export XPILOT_TEMPLATE_TEST_LOCK="$test_root/installing"

# Exercise the generated production rules without changing the enclosing build.
cp "$tray_build_dir/Makefile" "$test_root/Makefile"
make -C "$test_root" -j2 -o Makefile \
    "srcdir=$tray_source_dir" "top_builddir=$top_build_dir" \
    "DEPDIR=$tray_build_dir/.deps" "FUNCITY=$test_root/funcity" \
    'bindir=/opt/xpilot/bin' 'libexecdir=/opt/xpilot/libexec' \
    'pkgdatadir=/opt/xpilot/share' \
    org.xpilot.Infinity.ServerSettings1.service xpilot-infinity-tray.desktop

grep -Fx 'Exec="/opt/xpilot/libexec/xpilot-infinity-settings-helper"' \
    "$test_root/org.xpilot.Infinity.ServerSettings1.service"
grep -Fx 'Exec="/opt/xpilot/bin/xpilot-infinity-tray"' \
    "$test_root/xpilot-infinity-tray.desktop"
grep -Fx 'Icon=/opt/xpilot/share/icon-1254.png' \
    "$test_root/xpilot-infinity-tray.desktop"
echo 'Parallel tray template generation passed'
