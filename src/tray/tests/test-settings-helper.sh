#!/bin/sh
set -eu
test -f /run/systemd/container
config=/etc/default/xpilot-infinity-server
set -- $(sha256sum "$config")
generation=$1
before=$(cat "$config")
runuser -u xpilot-tray-no-agent -- /test-auth-cancel
test "$before" = "$(cat "$config")"
# A map-only permission must not grant general systemd control or free editing.
if runuser -u xpilot-tray-denied -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$generation" ndh.xp2; then
    echo 'Denied user changed configuration' >&2; exit 1
fi
test "$before" = "$(cat "$config")"
runuser -u xpilot-tray-map-only -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$generation" blood-music.xp2
/service-driver status | grep '^stopped '
set -- $(sha256sum "$config")
changed=$1
test "$generation" != "$changed"
test "$(head -1 /etc/default/.xpilot-infinity-settings-result)" = "$changed"
grep -F 'service remains stopped' /etc/default/.xpilot-infinity-settings-result
if runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$generation" ndh.xp2; then
    echo 'Stale configuration generation was accepted' >&2; exit 1
fi
if runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$changed" ../outside.xp2; then
    echo 'Map escaped the shared directory' >&2; exit 1
fi
/service-driver start
/contact-probe udp://127.0.0.1:15345 udp://127.0.0.1:15345
/contact-probe --status udp://127.0.0.1:15345 | grep -F "WORLD...........: Blood's Music"
pid=$(systemctl show xpilot-infinity-server --property=MainPID --value)
for user in xpilot-tray-map-only xpilot-tray-no-agent; do
    if runuser -u "$user" -- gdbus call --system \
        --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
        --method org.xpilot.Infinity.ServerSettings1.SelectMap "$changed" ndh.xp2; then
        echo 'Map authorization bypassed required restart authorization' >&2; exit 1
    fi
    test "$pid" = "$(systemctl show xpilot-infinity-server --property=MainPID --value)"
    set -- $(sha256sum "$config")
    test "$changed" = "$1"
done
runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$changed" ndh.xp2
/service-driver status | grep '^running '
/contact-probe udp://127.0.0.1:15345 udp://127.0.0.1:15345
/contact-probe --status udp://127.0.0.1:15345 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
# The service, not just the catalog/parser, must load a UTF-8 path with spaces.
data_directory=$(cat /xpilot-data-directory)
cp "$data_directory/maps/blood-music.xp2" "$data_directory/maps/日本語 map.xp2"
chmod 0644 "$data_directory/maps/日本語 map.xp2"
set -- $(sha256sum "$config")
runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$1" '日本語 map.xp2'
/contact-probe --status udp://127.0.0.1:15345 | grep -F "WORLD...........: Blood's Music"
/service-driver stop
set -- $(sha256sum "$config")
runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.SelectMap "$1" ndh.xp2
