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
/service-driver status | grep '^not-installed '
cp /xpilot-test-server.service /usr/lib/systemd/system/xpilot-infinity-server.service
systemctl daemon-reload
/service-driver status | grep '^stopped .*start=1 '
test "$(systemctl is-enabled xpilot-infinity-server || true)" = disabled
/service-driver start
pid=$(systemctl show -p MainPID --value xpilot-infinity-server)
test "$pid" -gt 0
/service-driver status | grep '^running '
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

# Request acceptance must not hide failure to execute the actual server.
mkdir -p /etc/systemd/system/xpilot-infinity-server.service.d
printf '%s\n' '[Service]' 'ExecStart=' 'ExecStart=/missing-xpilot-server' 'Restart=no' \
    > /etc/systemd/system/xpilot-infinity-server.service.d/failure.conf
systemctl daemon-reload
if /service-driver start; then echo 'Startup failure reported as success' >&2; exit 1; fi
/service-driver status | grep '^failed '
systemctl is-active dbus
