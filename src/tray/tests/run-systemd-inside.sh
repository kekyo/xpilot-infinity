#!/bin/sh
set -eu
test -f /run/systemd/container
test -f /xpilot-data-directory
exec > /xpilot-test-log 2>&1
finish()
{
    result=$?
    printf '%s\n' "$result" > /xpilot-test-result
    journalctl -u xpilot-infinity-server --no-pager || true
}
trap finish EXIT
data_directory=$(cat /xpilot-data-directory)
mkdir -p "$data_directory"
cp -a /xpilot-test-data/. "$data_directory/"
# Match installed package ownership/modes, not the developer checkout's umask.
chown -R root:root "$data_directory" /etc/default/xpilot-infinity-server
chmod -R go-w "$data_directory"
chmod 0644 /etc/default/xpilot-infinity-server
/service-driver status | grep '^not-installed '
cp /xpilot-test-server.service /usr/lib/systemd/system/xpilot-infinity-server.service
systemctl daemon-reload
/service-driver status | grep '^stopped .*start=1 '
test "$(systemctl is-enabled xpilot-infinity-server || true)" = disabled
/service-driver start
pid=$(systemctl show -p MainPID --value xpilot-infinity-server)
test "$pid" -gt 0
/service-driver status | grep '^running '
XPILOT_SERVICE_TEST_TIMEOUT_MS=15000 /service-driver reconnect
test "$pid" = "$(systemctl show -p MainPID --value xpilot-infinity-server)"
/contact-probe udp://127.0.0.1:15345 udp://127.0.0.1:15345

# External operations are observed through the same subscription as UI requests.
/service-driver wait-stopped &
observer=$!
systemctl stop xpilot-infinity-server
wait "$observer"
/service-driver wait-running &
observer=$!
systemctl start xpilot-infinity-server
wait "$observer"
/service-driver stop
/service-driver status | grep '^stopped '
systemctl mask xpilot-infinity-server
/service-driver status | grep '^stopped .*start=0 '
if /service-driver start; then echo 'Masked service started' >&2; exit 1; fi
systemctl unmask xpilot-infinity-server
XPILOT_SERVER_BINARY=/usr/games/xpilot-infinity-server sh /test-strict-map.sh

if test -x /test-desktop; then
    cp /xpilot-test-icon.png "$data_directory/icon-1254.png"
    useradd --create-home xpilot-tray-allowed
    useradd --create-home xpilot-tray-denied
    useradd --create-home xpilot-tray-map-only
    useradd --create-home xpilot-tray-no-agent
    mkdir -p /etc/polkit-1/rules.d
    cat > /etc/polkit-1/rules.d/00-xpilot-test.rules <<'EOF'
polkit.addRule(function(action, subject) {
    if (action.id == "org.xpilot.infinity.select-map" ||
        action.id == "org.xpilot.infinity.apply-configuration" ||
        (action.id == "org.freedesktop.systemd1.manage-units" &&
        action.lookup("unit") == "xpilot-infinity-server.service")) {
        if (subject.user == "xpilot-tray-map-only")
            return action.id == "org.xpilot.infinity.select-map" ? polkit.Result.YES : polkit.Result.NO;
        if (subject.user == "xpilot-tray-allowed") return polkit.Result.YES;
        if (subject.user == "xpilot-tray-denied") return polkit.Result.NO;
        if (subject.user == "xpilot-tray-no-agent") return polkit.Result.AUTH_ADMIN;
    }
});
EOF
    runuser -u xpilot-tray-denied -- xvfb-run -a /test-desktop /xpilot-infinity-tray denied
    /service-driver status | grep '^stopped '
    runuser -u xpilot-tray-no-agent -- xvfb-run -a /test-desktop /xpilot-infinity-tray denied
    /service-driver status | grep '^stopped '
    runuser -u xpilot-tray-allowed -- xvfb-run -a /test-desktop /xpilot-infinity-tray start
    /service-driver status | grep '^running '
    /contact-probe udp://127.0.0.1:15345 udp://127.0.0.1:15345
    runuser -u xpilot-tray-allowed -- xvfb-run -a /test-desktop /xpilot-infinity-tray fallback
    /service-driver status | grep '^running '
    runuser -u xpilot-tray-allowed -- xvfb-run -a /test-desktop /xpilot-infinity-tray stop
    /service-driver status | grep '^stopped '
fi

if test -x /xpilot-settings-helper; then
    cat > /usr/share/dbus-1/system-services/org.xpilot.Infinity.ServerSettings1.service <<'EOF'
[D-BUS Service]
Name=org.xpilot.Infinity.ServerSettings1
Exec=/xpilot-settings-helper
User=root
EOF
    systemctl reload dbus
    sh /test-settings-helper.sh
    sh /test-settings-editor.sh
    chmod 0755 /test-default-editor
    mkdir -p '/tmp/編集 test/config' '/tmp/編集 test/data/applications'
    cat > '/tmp/編集 test/data/applications/xpilot-test-editor.desktop' <<'EOF'
[Desktop Entry]
Type=Application
Name=XPilot test text editor
Exec=/test-default-editor %f
MimeType=text/plain;
Terminal=false
EOF
    cat > '/tmp/編集 test/config/mimeapps.list' <<'EOF'
[Default Applications]
text/plain=xpilot-test-editor.desktop;
EOF
    chown -R xpilot-tray-allowed:xpilot-tray-allowed '/tmp/編集 test'
    chmod -R go-rwx '/tmp/編集 test'
    runuser -u xpilot-tray-allowed -- env 'XDG_CONFIG_HOME=/tmp/編集 test/config' \
        'XDG_DATA_HOME=/tmp/編集 test/data' XPILOT_EDITOR_TEST_PORT=15346 \
        xvfb-run -a /test-desktop /xpilot-infinity-tray editor
    /service-driver status | grep '^stopped '
    /service-driver start
    /contact-probe --status udp://127.0.0.1:15346 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
    runuser -u xpilot-tray-allowed -- env 'XDG_CONFIG_HOME=/tmp/編集 test/config' \
        'XDG_DATA_HOME=/tmp/編集 test/data' XPILOT_EDITOR_TEST_PORT=15345 \
        xvfb-run -a /test-desktop /xpilot-infinity-tray editor
    /contact-probe --status udp://127.0.0.1:15345 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
    /service-driver stop
    runuser -u xpilot-tray-allowed -- xvfb-run -a /test-desktop /xpilot-infinity-tray map
    /service-driver status | grep '^stopped '
    /service-driver start
    /contact-probe --status udp://127.0.0.1:15345 | grep -F "WORLD...........: Blood's Music"
    runuser -u xpilot-tray-allowed -- xvfb-run -a /test-desktop /xpilot-infinity-tray map-running
    /contact-probe --status udp://127.0.0.1:15345 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
    /service-driver stop
fi

# Request acceptance must not hide failure to execute the actual server.
mkdir -p /etc/systemd/system/xpilot-infinity-server.service.d
printf '%s\n' '[Service]' 'ExecStart=' 'ExecStart=/missing-xpilot-server' 'Restart=no' \
    > /etc/systemd/system/xpilot-infinity-server.service.d/failure.conf
systemctl daemon-reload
if /service-driver start; then echo 'Startup failure reported as success' >&2; exit 1; fi
/service-driver status | grep '^failed '
systemctl is-active dbus
