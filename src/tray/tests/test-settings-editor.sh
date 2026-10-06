#!/bin/sh
set -eu
test -f /run/systemd/container
config=/etc/default/xpilot-infinity-server
set -- $(sha256sum "$config")
generation=$1
before=$(cat "$config")
restored="XPILOT_SERVER_OPTIONS='-noQuit +reportMeta -port 15345 -map ndh.xp2'"
snapshot="XPILOT_SERVER_OPTIONS='-noQuit +reportMeta -port 15346 -map ndh.xp2'"
if runuser -u xpilot-tray-map-only -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.Apply "$generation" "$snapshot"; then
    echo 'Map-only authorization allowed free-text editing' >&2; exit 1
fi
test "$before" = "$(cat "$config")"
runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.Apply "$generation" "$snapshot"
/service-driver status | grep '^stopped '
/service-driver start
/contact-probe --status udp://127.0.0.1:15346 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
set -- $(sha256sum "$config")
new_generation=$1
if runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.Apply "$generation" "$restored"; then
    echo 'Editing a stale generation overwrote the shared configuration' >&2; exit 1
fi
invalid=$(printf '\357\273\277map: ndh.xp2')
if runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.Apply "$new_generation" "$invalid"; then
    echo 'BOM-prefixed settings were accepted' >&2; exit 1
fi
test "$snapshot" = "$(cat "$config")"
runuser -u xpilot-tray-allowed -- gdbus call --system \
    --dest org.xpilot.Infinity.ServerSettings1 --object-path /org/xpilot/Infinity/ServerSettings \
    --method org.xpilot.Infinity.ServerSettings1.Apply "$new_generation" "$restored"
/contact-probe --status udp://127.0.0.1:15345 | grep -F 'WORLD...........: New Dark Hell-Next Generation'
/service-driver stop
