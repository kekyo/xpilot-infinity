#!/bin/sh

set -eu

usage()
{
    cat <<'EOF'
Usage: tests/run-wine-suite.sh --build-dir PATH --package-dir PATH \
       --installer PATH --wine-prefix PATH --arch ARCH [--jobs NUMBER]

Build and run the supported Windows unit tests, exercise the installer and
service, then test every contact/gameplay transport under Wine and Xvfb.
EOF
}

fail()
{
    echo "Wine test failure: $*" >&2
    if command -v dump_logs >/dev/null 2>&1; then
	dump_logs
    fi
    exit 1
}

build_dir=
package_dir=
installer=
wine_prefix=
architecture=
jobs=
inside_xvfb=false

while test "$#" -gt 0; do
    case "$1" in
        --build-dir)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            build_dir=$2
            shift 2
            ;;
        --package-dir)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            package_dir=$2
            shift 2
            ;;
        --installer)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            installer=$2
            shift 2
            ;;
        --wine-prefix)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            wine_prefix=$2
            shift 2
            ;;
        --arch)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            architecture=$2
            shift 2
            ;;
        --jobs)
            test "$#" -ge 2 || { usage >&2; exit 1; }
            jobs=$2
            shift 2
            ;;
        --inside-xvfb)
            inside_xvfb=true
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 1
            ;;
    esac
done

test -n "$build_dir" || { usage >&2; exit 1; }
test -n "$package_dir" || { usage >&2; exit 1; }
test -n "$installer" || { usage >&2; exit 1; }
test -n "$wine_prefix" || { usage >&2; exit 1; }
case "$architecture" in
    x86)
        wine_architecture=win32
        triplet=i686-w64-mingw32
        ;;
    x86_64)
        wine_architecture=win64
        triplet=x86_64-w64-mingw32
        ;;
    *) fail "--arch must be x86 or x86_64" ;;
esac
if test -z "$jobs"; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
fi
case "$jobs" in
    ''|0|*[!0-9]*) fail "--jobs must be a positive integer" ;;
esac

case "$build_dir" in
    /*) ;;
    *) fail "--build-dir must be an absolute path" ;;
esac
case "$package_dir" in
    /*) ;;
    *) fail "--package-dir must be an absolute path" ;;
esac
case "$installer" in
    /*) ;;
    *) fail "--installer must be an absolute path" ;;
esac
case "$wine_prefix" in
    /*) ;;
    *) fail "--wine-prefix must be an absolute path" ;;
esac
test -f "$installer" || fail "Windows installer is missing: $installer"

metadata_file="$build_dir/src/common/build-metadata.txt"
test -r "$metadata_file" \
    || fail "build metadata is unavailable: $metadata_file"
build_version=$(sed -n '1p' "$metadata_file")
case "$build_version" in
    ''|*[!0-9A-Za-z.+:~-]*) fail "invalid build version: $build_version" ;;
esac
build_commit=$(sed -n '2p' "$metadata_file")
case "$build_commit" in
    *[!0-9a-f]*) fail "invalid build commit ID: $build_commit" ;;
esac
test "${#build_commit}" -eq 40 \
    || fail "build commit ID must contain 40 hexadecimal characters"
build_title="XPilot Infinity [$build_version-$build_commit]"

make_program=${MAKE:-make}
wine_program=${WINE:-wine}
wineboot_program=${WINEBOOT:-wineboot}
wineserver_program=${WINESERVER:-wineserver}

if test "$inside_xvfb" = false; then
    for required_command in "$make_program" "$wine_program" \
        "$wineboot_program" "$wineserver_program" xvfb-run xdotool node file \
        timeout winepath; do
        command -v "$required_command" >/dev/null 2>&1 \
            || fail "required command was not found: $required_command"
    done
    command -v "$triplet-gcc" >/dev/null 2>&1 \
        || fail "required command was not found: $triplet-gcc"

    echo "===== build: supported Windows tests for $architecture ====="
    "$make_program" -C "$build_dir/tests" "-j$jobs" \
        test-framed-stream.exe \
        test-websocket-transport.exe \
        test-game-transport.exe \
        test-connect-target.exe \
        test-contact-target-probe.exe \
        test-transport-display.exe \
        test-socket-io.exe \
        test-sdl-versions.exe \
        test-score-font.exe \
        test-native-text.exe \
        test-native-socket-handle.exe
    "$make_program" -C "$build_dir/src/tray/tests" "-j$jobs" \
        test-service-win32.exe test-settings-acl-win32.exe test-settings-replace-win32.exe

    mkdir -p "$(dirname -- "$wine_prefix")"
    export WINEPREFIX=$wine_prefix
    export WINEARCH=$wine_architecture
    export WINEDEBUG=-all
    export LIBGL_ALWAYS_SOFTWARE=1
    exec xvfb-run -a \
        -s "-screen 0 1280x1024x24 +extension GLX +render -noreset" \
        /bin/sh "$0" \
            --inside-xvfb \
            --build-dir "$build_dir" \
            --package-dir "$package_dir" \
            --installer "$installer" \
            --wine-prefix "$wine_prefix" \
            --arch "$architecture" \
            --jobs "$jobs"
fi

export WINEPREFIX=$wine_prefix
export WINEARCH=$wine_architecture
export WINEDEBUG=-all
export LIBGL_ALWAYS_SOFTWARE=1

runtime_dir=$(mktemp -d \
    "${TMPDIR:-/tmp}/xpilot-wine-$architecture.XXXXXX")
runtime_package="$runtime_dir/package"
server_pid=
client_pid=
meta_report_fixture_pid=
server_log=
client_log=
window_id=
windows_service_registered=false
windows_service_name=XPilotInfinityServer
installer_uninstaller=
installer_server_config=

dump_logs()
{
    if test -n "${runtime_dir:-}" && test -d "$runtime_dir"; then
        for log_file in "$runtime_dir"/*.log; do
            if test -f "$log_file"; then
                echo "===== $log_file =====" >&2
                sed -n '1,240p' "$log_file" >&2
            fi
        done
    fi
}

stop_wine_server()
{
    "$wineserver_program" -k >>"$runtime_dir/wineserver.log" 2>&1 || true
    timeout 30s "$wineserver_program" -w \
        >>"$runtime_dir/wineserver.log" 2>&1
}

cleanup()
{
    if test -n "$installer_uninstaller" \
        && test -f "$installer_uninstaller"; then
	timeout 90s "$wine_program" "$installer_uninstaller" /S \
	    >>"$runtime_dir/installer-cleanup.log" 2>&1 || true
    fi
    if test "$windows_service_registered" = true; then
	timeout 15s "$wine_program" sc.exe stop "$windows_service_name" \
	    >>"$runtime_dir/windows-service-cleanup.log" 2>&1 || true
	timeout 15s "$wine_program" sc.exe delete "$windows_service_name" \
	    >>"$runtime_dir/windows-service-cleanup.log" 2>&1 || true
    fi
    cleanup_deadline=$(($(date +%s) + 10))
    for process_id in "$client_pid" "$server_pid" \
        "$meta_report_fixture_pid"; do
        if test -n "$process_id" && kill -0 "$process_id" 2>/dev/null; then
            kill -TERM "$process_id" 2>/dev/null || true
        fi
    done
    while :; do
        cleanup_running=false
        for process_id in "$client_pid" "$server_pid" \
            "$meta_report_fixture_pid"; do
            if test -n "$process_id" \
                && kill -0 "$process_id" 2>/dev/null; then
                cleanup_running=true
            fi
        done
        test "$cleanup_running" = false && break
        if test "$(date +%s)" -ge "$cleanup_deadline"; then
            for process_id in "$client_pid" "$server_pid" \
                "$meta_report_fixture_pid"; do
                if test -n "$process_id" \
                    && kill -0 "$process_id" 2>/dev/null; then
                    kill -KILL "$process_id" 2>/dev/null || true
                fi
            done
            break
        fi
        sleep 0.1
    done
    for process_id in "$client_pid" "$server_pid" \
        "$meta_report_fixture_pid"; do
        if test -n "$process_id"; then
            wait "$process_id" 2>/dev/null || true
        fi
    done
    stop_wine_server || true
    case "$runtime_dir" in
        "${TMPDIR:-/tmp}"/xpilot-wine-*) rm -rf -- "$runtime_dir" ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

wait_until()
{
    wait_description=$1
    wait_seconds=$2
    shift 2
    wait_deadline=$(($(date +%s) + wait_seconds))
    while ! "$@"; do
        if test "$(date +%s)" -ge "$wait_deadline"; then
            fail "timed out waiting for $wait_description"
        fi
        sleep 0.1
    done
}

process_stopped()
{
    ! kill -0 "$1" 2>/dev/null
}

server_ready()
{
    if ! kill -0 "$server_pid" 2>/dev/null; then
        fail "server stopped before becoming ready"
    fi
    grep -q 'Server runs at' "$server_log" 2>/dev/null
}

client_joined()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
        fail "client stopped before joining the server"
    fi
    grep -q '\*\*\* Login allowed\.' "$client_log" 2>/dev/null \
        && grep -q "Welcome .*$client_name" "$server_log" 2>/dev/null
}

client_transport_banner_reported()
{
    grep -Fq "*** Connected to 127.0.0.1 "\
"[Contact/Lobby: $expected_contact_transport, "\
"Gameplay: $expected_gameplay_transport]" "$client_log" 2>/dev/null
}

find_game_window()
{
    window_id=$(xdotool search --onlyvisible --name '^XPilot Infinity ' \
        2>/dev/null | tail -n 1 || true)
    test -n "$window_id"
}

find_connection_failure_window()
{
    window_id=$(xdotool search --onlyvisible \
	--name '^XPilot Infinity - Connection failed$' 2>/dev/null \
	| tail -n 1 || true)
    test -n "$window_id"
}

connection_failure_window_visible()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "client stopped before showing the connection failure"
    fi
    find_connection_failure_window
}

game_window_transport_visible()
{
    game_window_title=$(xdotool getwindowname "$window_id" 2>/dev/null \
	|| true)
    test "$game_window_title" = \
	"$build_title - 127.0.0.1 "\
"[Gameplay: $expected_gameplay_transport]"
}

quit_game_client()
{
    quit_case=$1
    quit_deadline=$(($(date +%s) + 30))
    quit_attempted=false
    while kill -0 "$client_pid" 2>/dev/null \
        && test "$(date +%s)" -lt "$quit_deadline"; do
        if find_game_window; then
            quit_attempted=true
            xdotool key --clearmodifiers --delay 150 \
                --window "$window_id" Escape y >/dev/null 2>&1 \
                || true
        fi
        sleep 0.25
    done
    $quit_attempted \
        || fail "could not request a graceful $quit_case client quit"
    if kill -0 "$client_pid" 2>/dev/null; then
        fail "$quit_case client did not stop after graceful quit requests"
    fi
}

client_departed()
{
    grep -q "Goodbye .*$client_name" "$server_log" 2>/dev/null
}

meta_report_fixture_ready()
{
    if ! kill -0 "$meta_report_fixture_pid" 2>/dev/null; then
        fail "local metaserver report fixture stopped before listening"
    fi
    test -s "$runtime_dir/meta-report-fixture.port"
}

meta_tcp_transport_reported()
{
    test -s "$runtime_dir/meta-report-fixture.received" \
        && grep -q "^source-port $meta_report_contact_port$" \
            "$runtime_dir/meta-report-fixture.received" \
        && grep -Fqx "add version ${build_version}infinity+ct=tcp+gt=udp" \
            "$runtime_dir/meta-report-fixture.received"
}

reserve_contact_port()
{
    node -e '
const dgram = require("dgram");
const net = require("net");
const reserve = () => {
  const tcp = net.createServer();
  tcp.once("error", reserve);
  tcp.listen(0, "127.0.0.1", () => {
    const port = tcp.address().port;
    const udp = dgram.createSocket("udp4");
    udp.once("error", () => tcp.close(reserve));
    udp.bind(port, "127.0.0.1", () => {
      process.stdout.write(String(port));
      udp.close();
      tcp.close();
    });
  });
};
reserve();'
}

run_wine_unit_test()
{
    test_name=$1
    test_executable="$build_dir/tests/$test_name.exe"
    test_log="$runtime_dir/$test_name.log"
    test -f "$test_executable" \
        || fail "Windows test executable is missing: $test_executable"
    if ! timeout 30s "$wine_program" "$test_executable" \
        >"$test_log" 2>&1; then
        fail "$test_name failed under Wine"
    fi
}

stop_server()
{
    kill -TERM "$server_pid" 2>/dev/null || true
    wait_until "server shutdown" 15 process_stopped "$server_pid"
    wait "$server_pid" 2>/dev/null || true
    server_pid=
}

run_executable_relative_resources()
{
    contact_port=$(reserve_contact_port)
    foreign_directory="$runtime_dir/foreign-working-directory"
    server_log="$runtime_dir/server-executable-relative-resources.log"
    mkdir -p "$foreign_directory"

    (
	cd "$foreign_directory"
	exec "$wine_program" \
	    "$runtime_package/xpilot-infinity-server.exe" \
	    -map ndh.xp2 -port "$contact_port" \
	    -noQuit +reportMeta -transport udp
    ) >"$server_log" 2>&1 &
    server_pid=$!
    wait_until "executable-relative packaged resources" 30 server_ready
    stop_server
}

windows_service_stopped()
{
    timeout 15s "$wine_program" sc.exe query "$windows_service_name" \
	>"$runtime_dir/windows-service-query.log" 2>&1 \
	&& grep -Eq 'STATE[[:space:]]*:[[:space:]]*1[[:space:]]+STOPPED' \
	    "$runtime_dir/windows-service-query.log"
}

windows_service_ready()
{
    timeout 15s "$wine_program" sc.exe query "$windows_service_name" \
	>"$runtime_dir/windows-service-running.log" 2>&1 \
	&& grep -Eq 'STATE[[:space:]]*:[[:space:]]*4[[:space:]]+RUNNING' \
	    "$runtime_dir/windows-service-running.log"
}

windows_service_absent()
{
    ! timeout 15s "$wine_program" sc.exe query "$windows_service_name" \
	>"$runtime_dir/windows-service-absent.log" 2>&1
}

path_absent()
{
    test ! -e "$1"
}

installed_client_stopped()
{
    if ! timeout 15s "$wine_program" tasklist.exe \
	>"$runtime_dir/installer-client-tasklist.log" 2>&1; then
	return 1
    fi
    ! grep -Fqi 'xpilot-infinity-sdl.exe' \
	"$runtime_dir/installer-client-tasklist.log"
}

run_start_menu_shortcut()
{
    shortcut_path=$1
    shortcut_windows=$(winepath -w "$shortcut_path")
    client_log="$runtime_dir/installer-start-menu-client.log"

    "$wine_program" start.exe /wait "$shortcut_windows" \
	>"$client_log" 2>&1 &
    client_pid=$!
    wait_until "installed Start menu client window" 30 find_game_window
    installed_window_title=$(xdotool getwindowname "$window_id" 2>/dev/null \
	|| true)
    test "$installed_window_title" = "$build_title" \
	|| fail "Start menu shortcut did not open the server browser"
    timeout 15s "$wine_program" taskkill.exe /F \
	/IM xpilot-infinity-sdl.exe \
	>"$runtime_dir/installer-client-stop.log" 2>&1 \
	|| fail "could not stop the installed server browser after inspection"
    wait_until "installed server browser process shutdown" 20 \
	installed_client_stopped
    wait_until "Start menu launcher shutdown" 20 \
	process_stopped "$client_pid"
    wait "$client_pid" 2>/dev/null || true
    client_pid=
    window_id=
}

run_windows_service_case()
{
    windows_service_stopped \
	|| fail "installed Windows service started before it was requested"

    timeout 30s "$wine_program" sc.exe start "$windows_service_name" \
	>"$runtime_dir/windows-service-start.log" 2>&1 \
	|| fail "Windows SCM could not start the server"
    wait_until "Windows service server readiness" 30 windows_service_ready

    timeout 30s "$wine_program" \
	"$build_dir/tests/test-contact-target-probe.exe" \
	--interactive \
	"udp://127.0.0.1:$contact_port" \
	>"$runtime_dir/windows-service-contact.log" 2>&1 \
	|| fail "Windows service server did not answer contact requests"

    timeout 15s "$wine_program" sc.exe stop "$windows_service_name" \
	>"$runtime_dir/windows-service-stop.log" 2>&1 \
	|| fail "Windows SCM could not request server shutdown"
    wait_until "Windows service server shutdown" 30 windows_service_stopped
}

run_windows_installer_cases()
{
    default_install_name="xpilot-installer-default-$architecture"
    service_install_name="xpilot-installer-service-$architecture"
    default_install_dir="$wine_prefix/drive_c/$default_install_name"
    service_install_dir="$wine_prefix/drive_c/$service_install_name"
    default_windows_dir="C:\\$default_install_name"
    service_windows_dir="C:\\$service_install_name"
    installer_shortcut="$wine_prefix/drive_c/ProgramData/Microsoft/Windows/Start Menu/Programs/XPilot Infinity/XPilot Infinity.lnk"

    if test -f "$default_install_dir/uninstall.exe"; then
	timeout 90s "$wine_program" "$default_install_dir/uninstall.exe" /S \
	    >"$runtime_dir/default-installer-preclean.log" 2>&1 \
	    || fail "could not remove a previous default installer test"
    fi
    if test -f "$service_install_dir/uninstall.exe"; then
	timeout 90s "$wine_program" "$service_install_dir/uninstall.exe" /S \
	    >"$runtime_dir/service-installer-preclean.log" 2>&1 \
	    || fail "could not remove a previous service installer test"
    fi
    if ! windows_service_absent; then
	timeout 15s "$wine_program" sc.exe stop "$windows_service_name" \
	    >>"$runtime_dir/windows-service-preclean.log" 2>&1 || true
	timeout 15s "$wine_program" sc.exe delete "$windows_service_name" \
	    >>"$runtime_dir/windows-service-preclean.log" 2>&1 \
	    || fail "could not remove a previous installer test service"
    fi
    test ! -e "$default_install_dir" \
	|| fail "stale default installer test directory remains"
    test ! -e "$service_install_dir" \
	|| fail "stale service installer test directory remains"

    echo "===== test: default Windows installer for $architecture ====="
    timeout 90s "$wine_program" "$installer" /S \
	"/D=$default_windows_dir" \
	>"$runtime_dir/default-installer.log" 2>&1 \
	|| fail "default Windows installer failed"
    installer_uninstaller="$default_install_dir/uninstall.exe"
    for installed_file in xpilot-infinity-server.exe \
	xpilot-infinity-sdl.exe COPYING icon.ico uninstall.exe; do
	test -f "$default_install_dir/$installed_file" \
	    || fail "default installer omitted $installed_file"
    done
    test -f "$installer_shortcut" \
	|| fail "default installer did not create the Start menu shortcut"
    windows_service_absent \
	|| fail "default installer registered the optional server service"
    run_start_menu_shortcut "$installer_shortcut"

    timeout 90s "$wine_program" "$installer_uninstaller" /S \
	>"$runtime_dir/default-uninstaller.log" 2>&1 \
	|| fail "default Windows uninstaller failed"
    installer_uninstaller=
    wait_until "default installation removal" 30 \
	path_absent "$default_install_dir"
    test ! -e "$installer_shortcut" \
	|| fail "default uninstaller left the Start menu shortcut behind"

    contact_port=$(reserve_contact_port)
    installer_server_data="$wine_prefix/drive_c/ProgramData/XPilot Infinity/server"
    installer_server_config="$installer_server_data/xpilot-infinity-server.conf"
    installer_server_log="$installer_server_data/xpilot-infinity-server.log"
    mkdir -p "$installer_server_data"
    printf '%s\n' \
	'# Preserved by the XPilot Infinity installer test.' \
	'map: ndh.xp2' \
	'noQuit: true' \
	'reportMeta: false' \
	"port: $contact_port" >"$installer_server_config"

    echo "===== test: optional Windows service installer for $architecture ====="
    timeout 90s "$wine_program" "$installer" /S /SERVER_SERVICE=1 \
	"/D=$service_windows_dir" \
	>"$runtime_dir/service-installer.log" 2>&1 \
	|| fail "Windows service installer failed"
    installer_uninstaller="$service_install_dir/uninstall.exe"
    test -f "$service_install_dir/xpilot-infinity-server.exe" \
	|| fail "service installer omitted the server executable"
    test -f "$installer_shortcut" \
	|| fail "service installer omitted the Start menu shortcut"
    grep -Fq '# Preserved by the XPilot Infinity installer test.' \
	"$installer_server_config" \
	|| fail "service installer overwrote the existing server configuration"

    timeout 15s "$wine_program" reg.exe query \
	"HKLM\\System\\CurrentControlSet\\Services\\$windows_service_name" \
	>"$runtime_dir/windows-service-config.log" 2>&1 \
	|| fail "installer did not register the Windows service"
    windows_service_registered=true
    grep -Eq 'Start[[:space:]]+REG_DWORD[[:space:]]+0x3' \
	"$runtime_dir/windows-service-config.log" \
	|| fail "installed Windows service was not configured for manual start"
    grep -Eq 'ObjectName[[:space:]]+REG_SZ[[:space:]]+NT AUTHORITY\\LocalService' \
	"$runtime_dir/windows-service-config.log" \
	|| fail "installed Windows service does not use LocalService"
    grep -Fq "C:\\$service_install_name\\xpilot-infinity-server.exe" \
	"$runtime_dir/windows-service-config.log" \
	|| fail "installed Windows service executable path is incorrect"
    grep -Fq -- '--windows-service' \
	"$runtime_dir/windows-service-config.log" \
	|| fail "installed server was not configured for service dispatch"
    windows_service_stopped \
	|| fail "installer started the optional Windows service"

    run_windows_service_case

    test -f "$installer_server_log" \
	|| fail "Windows service did not write its configured log"

    timeout 90s "$wine_program" "$installer_uninstaller" /S \
	>"$runtime_dir/service-uninstaller.log" 2>&1 \
	|| fail "Windows service uninstaller failed"
    installer_uninstaller=
    wait_until "service installation removal" 30 \
	path_absent "$service_install_dir"
    wait_until "installed Windows service removal" 30 \
	windows_service_absent
    windows_service_registered=false
    test ! -e "$installer_shortcut" \
	|| fail "service uninstaller left the Start menu shortcut behind"
    grep -Fq '# Preserved by the XPilot Infinity installer test.' \
	"$installer_server_config" \
	|| fail "uninstaller removed the preserved server configuration"
}

run_contact_target_failover()
{
    contact_port=$(reserve_contact_port)
    server_log="$runtime_dir/server-contact-target-failover.log"
    probe_log="$runtime_dir/contact-target-failover.log"
    list_log="$runtime_dir/client-contact-target-list.log"

    (
        cd "$runtime_package"
        exec "$wine_program" ./xpilot-infinity-server.exe \
            -map lib/maps/ndh.xp2 \
            -port "$contact_port" \
            -noQuit +reportMeta \
            -contactTransport tcp -gameTransport tcp -udp
    ) >"$server_log" 2>&1 &
    server_pid=$!
    wait_until "contact target failover server readiness" 30 server_ready

    timeout 60s "$wine_program" \
        "$build_dir/tests/test-contact-target-probe.exe" \
        "tcp://127.0.0.1:$contact_port" \
        "udp://127.0.0.1:$contact_port" >"$probe_log" 2>&1 \
        || fail "TCP target failure did not continue to the UDP target"
    test "$(grep -Fc 'Contacting server 127.0.0.1.' "$probe_log")" -ge 2 \
        || fail "contact target probe did not attempt the endpoints"
    grep -Fq '[Contact/Lobby: UDP, Gameplay: UDP]' "$probe_log" \
        || fail "contact target probe did not establish the UDP endpoint"

    (
	cd "$runtime_package"
	exec "$wine_program" ./xpilot-infinity-sdl.exe -list \
	    "tcp://127.0.0.1:$contact_port" \
	    "udp://127.0.0.1:$contact_port"
    ) >"$list_log" 2>&1 \
	|| fail "server listing did not preserve a contacted fallback result"
    grep -Fq 'TRANSPORTS.......: UDP -> UDP' "$list_log" \
	|| fail "server listing did not report the responding UDP endpoint"
    if grep -Fq 'ERROR: Connection failed:' "$list_log"; then
	fail "server listing response was reported as a connection failure"
    fi
    if find_connection_failure_window; then
	fail "server listing displayed a connection failure dialog"
    fi
    stop_server
}

run_connection_failure_notification()
{
    contact_port=$(reserve_contact_port)
    server_log="$runtime_dir/server-connection-failure.log"
    client_log="$runtime_dir/client-connection-failure.log"

    (
	cd "$runtime_package"
	exec "$wine_program" ./xpilot-infinity-server.exe \
	    -map lib/maps/ndh.xp2 -port "$contact_port" \
	    -noQuit +reportMeta -transport udp
    ) >"$server_log" 2>&1 &
    server_pid=$!
    wait_until "UDP-only failure fixture readiness" 30 server_ready

    (
	cd "$runtime_package"
	exec "$wine_program" ./xpilot-infinity-sdl.exe \
	    "tcp://127.0.0.1:$contact_port"
    ) >"$client_log" 2>&1 &
    client_pid=$!
    wait_until "connection failure dialog" 45 \
	connection_failure_window_visible

    kill -0 "$client_pid" 2>/dev/null \
	|| fail "client exited while the connection failure dialog was visible"
    grep -Fq "Could not contact 127.0.0.1:$contact_port." "$client_log" \
	|| fail "final connection failure did not identify the endpoint"
    grep -Fq 'Contact/Lobby: TCP' "$client_log" \
	|| fail "final connection failure omitted the contact transport"
    grep -Fq 'Gameplay: TCP' "$client_log" \
	|| fail "final connection failure omitted the gameplay transport"
    grep -Fq "ERROR: Can't contact 127.0.0.1 on port $contact_port" \
	"$client_log" \
	|| fail "Windows connection attempt diagnostics were not written"

    failure_window_count=$(xdotool search --onlyvisible \
	--name '^XPilot Infinity - Connection failed$' 2>/dev/null \
	| wc -l)
    test "$failure_window_count" -eq 1 \
	|| fail "expected one final connection failure dialog"

    xdotool key --clearmodifiers --window "$window_id" Return \
	>/dev/null 2>&1 \
	|| fail "could not dismiss the connection failure dialog"
    wait_until "client exit after failure acknowledgement" 15 \
	process_stopped "$client_pid"
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    test "$client_status" -ne 0 \
	|| fail "connection failure returned a successful exit status"

    window_id=
    stop_server
}

run_meta_report_case()
{
    node -e '
const dgram = require("node:dgram");
const fs = require("node:fs");
const portFile = process.argv[1];
const receivedFile = process.argv[2];
const socket = dgram.createSocket("udp4");
socket.on("error", (error) => {
  process.stderr.write(`${error.stack}\n`);
  socket.close(() => process.exit(1));
});
socket.on("message", (message, remote) => {
  const payload = message.toString("utf8").replace(/\0+$/, "");
  fs.appendFileSync(
    receivedFile,
    `source-port ${remote.port}\n${payload}\n---\n`,
  );
});
socket.bind(0, "127.0.0.1", () => {
  fs.writeFileSync(portFile, String(socket.address().port));
});
const stop = () => socket.close(() => process.exit(0));
process.on("SIGTERM", stop);
process.on("SIGINT", stop);
' "$runtime_dir/meta-report-fixture.port" \
        "$runtime_dir/meta-report-fixture.received" \
        >"$runtime_dir/meta-report-fixture.log" 2>&1 &
    meta_report_fixture_pid=$!
    wait_until "local metaserver report fixture" 10 \
        meta_report_fixture_ready

    meta_report_contact_port=$(reserve_contact_port)
    meta_report_port=$(sed -n '1p' \
        "$runtime_dir/meta-report-fixture.port")
    server_log="$runtime_dir/server-meta-tcp-contact.log"
    (
        cd "$runtime_package"
        XPILOT_META_REPORT_HOST=127.0.0.1 \
        XPILOT_META_REPORT_HOST_TWO=127.0.0.1 \
        XPILOT_META_REPORT_PORT="$meta_report_port" \
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 \
                -port "$meta_report_contact_port" \
                -noQuit -reportMeta \
                -contactTransport tcp -gameTransport udp
    ) >"$server_log" 2>&1 &
    server_pid=$!
    wait_until "TCP-contact metaserver report startup" 30 server_ready
    wait_until "TCP-contact metaserver transport advertisement" 20 \
        meta_tcp_transport_reported
    stop_server

    kill -TERM "$meta_report_fixture_pid" 2>/dev/null || true
    wait "$meta_report_fixture_pid" 2>/dev/null || true
    meta_report_fixture_pid=
}

run_gameplay_case()
{
    contact_transport=$1
    gameplay_transport=$2
    client_user=EntryUser
    expected_contact_transport=$(printf '%s' "$contact_transport" \
	| tr '[:lower:]' '[:upper:]')
    expected_gameplay_transport=$(printf '%s' "$gameplay_transport" \
	| tr '[:lower:]' '[:upper:]')
    if test "$contact_transport" = websocket; then
	expected_contact_transport=WebSocket
    fi
    if test "$gameplay_transport" = websocket; then
	expected_gameplay_transport=WebSocket
    fi
    transport_case="$contact_transport-contact-$gameplay_transport-game"
    contact_port=$(reserve_contact_port)
    server_log="$runtime_dir/server-$transport_case.log"
    client_log="$runtime_dir/client-$transport_case.log"
    case "$architecture:$contact_transport:$gameplay_transport" in
	x86:udp:udp) client_name=W32UU ;;
	x86:udp:tcp) client_name=W32UT ;;
	x86:tcp:udp) client_name=W32TU ;;
	x86:tcp:tcp) client_name=W32TT ;;
	x86:websocket:websocket) client_name=W32WS ;;
	x86_64:udp:udp) client_name=W64UU ;;
	x86_64:udp:tcp) client_name=W64UT ;;
	x86_64:tcp:udp) client_name=W64TU ;;
	x86_64:tcp:tcp) client_name=W64TT ;;
	x86_64:websocket:websocket) client_name=W64WS ;;
    esac

    # Keep Wine's process-wide audio and socket state from leaking between
    # gameplay cases.  Sound is not part of these transport/UI assertions.
    stop_wine_server \
	|| fail "Wine server did not stop before $transport_case"

    (
        cd "$runtime_package"
        case "$contact_transport:$gameplay_transport" in
        tcp:tcp)
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 -port "$contact_port" \
                -noQuit +reportMeta -transport tcp
            ;;
        websocket:websocket)
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 -port "$contact_port" \
                -noQuit +reportMeta -websocket
            ;;
        udp:udp)
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 -port "$contact_port" \
                -noQuit +reportMeta \
                -contactTransport tcp -gameTransport tcp -udp
            ;;
        tcp:udp)
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 -port "$contact_port" \
                -noQuit +reportMeta -tcp -gameTransport udp
            ;;
        udp:tcp)
            exec "$wine_program" ./xpilot-infinity-server.exe \
                -map lib/maps/ndh.xp2 -port "$contact_port" \
                -noQuit +reportMeta -transport udp -gameTransport tcp
            ;;
        esac
    ) >"$server_log" 2>&1 &
    server_pid=$!
    wait_until "$transport_case server readiness" 30 server_ready

    (
        cd "$runtime_package"
        case "$contact_transport:$gameplay_transport" in
        tcp:tcp)
            exec "$wine_program" ./xpilot-infinity-sdl.exe \
                -geometry 800x600 -join -sound false \
		-name "$client_name" \
                -user "$client_user" -messagesToStdout 2 \
                "tcp://127.0.0.1:$contact_port"
            ;;
        websocket:websocket)
            exec "$wine_program" ./xpilot-infinity-sdl.exe \
                -geometry 800x600 -join -sound false \
		-name "$client_name" \
                -user "$client_user" -messagesToStdout 2 \
                "ws://127.0.0.1:$contact_port"
            ;;
        udp:udp)
            exec "$wine_program" ./xpilot-infinity-sdl.exe \
                -geometry 800x600 -join -sound false \
		-name "$client_name" \
                -user "$client_user" -messagesToStdout 2 \
                -contactTransport tcp -gameTransport tcp \
                "udp://127.0.0.1:$contact_port"
            ;;
        *)
            exec "$wine_program" ./xpilot-infinity-sdl.exe \
                -geometry 800x600 -join -sound false \
		-port "$contact_port" \
                -name "$client_name" -user "$client_user" \
                -messagesToStdout 2 \
                -contactTransport "$contact_transport" \
                -gameTransport "$gameplay_transport" \
                127.0.0.1
            ;;
        esac
    ) >"$client_log" 2>&1 &
    client_pid=$!

    wait_until "$transport_case client login" 30 client_joined
    wait_until "$transport_case first-player entry announcement" 15 \
	grep -Fq "$client_name ($client_user) has entered" "$client_log"
    wait_until "$transport_case connection transport banner" 15 \
	client_transport_banner_reported
    wait_until "$transport_case SDL window" 30 find_game_window
    wait_until "$transport_case gameplay transport window title" 15 \
	game_window_transport_visible
    wait_until "$transport_case OpenGL context" 30 \
        grep -q '^OpenGL context:' "$client_log"
    wait_until "$transport_case text renderers" 30 \
        grep -q '^Font text renderers ready: game=renderer map=renderer' \
            "$client_log"

    quit_game_client "$transport_case"
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    test "$client_status" -eq 0 \
        || fail "$transport_case client returned status $client_status"
    wait_until "$transport_case server departure" 10 client_departed

    if grep -q 'accept error' "$server_log"; then
        fail "$transport_case server reported an accept failure"
    fi
    stop_server
    window_id=
}

server_executable="$package_dir/xpilot-infinity-server.exe"
client_executable="$package_dir/xpilot-infinity-sdl.exe"
map_file="$package_dir/lib/maps/ndh.xp2"
openal_library="$package_dir/OpenAL32.dll"
freealut_library=
for freealut_name in alut.dll libalut.dll freealut.dll; do
    if test -f "$package_dir/$freealut_name"; then
        freealut_library="$package_dir/$freealut_name"
        break
    fi
done
test -n "$freealut_library" \
    || fail "packaged Windows freealut runtime is missing"
for required_file in "$server_executable" "$client_executable" "$map_file" \
    "$openal_library" "$freealut_library"; do
    test -f "$required_file" \
        || fail "packaged Windows test input is missing: $required_file"
done

case "$architecture" in
    x86)
        file "$server_executable" | grep -q 'PE32 executable' \
            || fail "x86 server is not a PE32 executable"
        file "$server_executable" | grep -qv 'PE32+' \
            || fail "x86 server unexpectedly uses PE32+"
        ;;
    x86_64)
        file "$server_executable" | grep -q 'PE32+ executable' \
            || fail "x86_64 server is not a PE32+ executable"
        ;;
esac
file "$installer" | grep -q 'Nullsoft Installer' \
    || fail "Windows installer is not an NSIS executable"

mkdir -p "$runtime_package"
cp -R "$package_dir/." "$runtime_package/"

echo "===== initialize: Wine $architecture prefix ====="
timeout 60s "$wineboot_program" -u >"$runtime_dir/wineboot.log" 2>&1 \
    || fail "Wine prefix initialization failed"
timeout 30s "$wineserver_program" -w >>"$runtime_dir/wineboot.log" 2>&1 \
    || fail "Wine prefix initialization did not settle"

run_windows_installer_cases

echo "===== test: Windows service state reporting for $architecture ====="
timeout 30s "$wine_program" "$build_dir/src/tray/tests/test-service-win32.exe" \
    >"$runtime_dir/test-service-win32.log" 2>&1 \
    || fail "Windows service state reporting failed"
echo "===== test: Windows settings permissions for $architecture ====="
timeout 30s "$wine_program" "$build_dir/src/tray/tests/test-settings-acl-win32.exe" \
    >"$runtime_dir/test-settings-acl-win32.log" 2>&1 \
    || fail "Windows settings permissions failed"
echo "===== test: Windows settings replacement for $architecture ====="
timeout 30s "$wine_program" "$build_dir/src/tray/tests/test-settings-replace-win32.exe" \
    >"$runtime_dir/test-settings-replace-win32.log" 2>&1 \
    || fail "Windows settings replacement failed"

for unit_test in test-framed-stream test-websocket-transport \
    test-game-transport test-connect-target \
    test-transport-display test-socket-io test-sdl-versions \
    test-score-font test-native-text test-native-socket-handle; do
    run_wine_unit_test "$unit_test"
done

run_executable_relative_resources
run_contact_target_failover
run_connection_failure_notification
run_meta_report_case
for contact_transport in udp tcp; do
    for gameplay_transport in udp tcp; do
        run_gameplay_case "$contact_transport" "$gameplay_transport"
    done
done
run_gameplay_case websocket websocket

stop_wine_server || fail "Wine server did not stop after tests"
echo "Wine $architecture unit, metaserver, and UDP/TCP/WebSocket integration tests passed"
