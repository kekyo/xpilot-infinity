#!/bin/sh

set -eu

if test "${1:-}" != --inside-xvfb; then
    for command_name in xvfb-run xdotool xwininfo node stdbuf; do
        if ! command -v "$command_name" >/dev/null 2>&1; then
            echo "Missing E2E dependency: $command_name" >&2
            exit 1
        fi
    done
    if test -n "${XPILOT_E2E_SCREENSHOT_DIR:-}"; then
        for command_name in xwd ffmpeg; do
            if ! command -v "$command_name" >/dev/null 2>&1; then
                echo "Missing screenshot dependency: $command_name" >&2
                exit 1
            fi
        done
    fi

    case "${XPILOT_TEST_PREFIX:?XPILOT_TEST_PREFIX is required}" in
        /tmp/*|/var/tmp/*)
            ;;
        *)
            echo "SKIP: configure with a prefix below /tmp or /var/tmp for E2E" >&2
            exit 77
            ;;
    esac
    export LIBGL_ALWAYS_SOFTWARE=1
    export SDL_VIDEODRIVER=x11
    exec xvfb-run -a -s "-screen 0 1280x1024x24 +extension GLX +render -noreset" \
        /bin/sh "$0" --inside-xvfb
fi

client="${XPILOT_TEST_BINDIR:?XPILOT_TEST_BINDIR is required}/xpilot-infinity-sdl"
x11_client="${XPILOT_TEST_BINDIR}/xpilot-infinity-x11"
server="${XPILOT_TEST_BINDIR}/xpilot-infinity-server"
map="${XPILOT_TEST_PKGDATADIR:?XPILOT_TEST_PKGDATADIR is required}/maps/ndh.xp2"
contact_target_probe="${XPILOT_CONTACT_TARGET_PROBE:?XPILOT_CONTACT_TARGET_PROBE is required}"

for required_file in "$client" "$server" "$map" "$contact_target_probe"; do
    if test ! -r "$required_file"; then
        echo "Installed E2E input is missing: $required_file" >&2
        exit 1
    fi
done

product_name='XPilot Infinity'
server_title=$("$server" -version \
    | tr -d '\r' \
    | sed -n '/^XPilot Infinity /p' \
    | tail -n 1 \
    || true)
case "$server_title" in
    "$product_name ["*-*"]")
        build_identity=${server_title#"$product_name ["}
        build_identity=${build_identity%]}
        build_commit=${build_identity##*-}
        build_version=${build_identity%"-$build_commit"}
        ;;
    *)
        echo "Unexpected server title: $server_title" >&2
        exit 1
        ;;
esac
test -n "$build_version" || {
    echo "The server version is empty" >&2
    exit 1
}
test "${#build_commit}" -eq 40 || {
    echo "The server commit ID is not a full SHA-1 hash: $build_commit" >&2
    exit 1
}
case $build_commit in
    *[!0-9a-f]*)
        echo "The server commit ID is not hexadecimal: $build_commit" >&2
        exit 1
        ;;
esac

runtime_dir=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-sdl3-e2e.XXXXXX")
meta_pid=
meta_fixture_pid=
meta_report_fixture_pid=
server_pid=
client_pid=
tcp_proxy_pid=
multiple_client_pids=
stalled_pid=
window_id=
window_owner_pid=
game_server_log=
game_client_log=
game_client_name=
game_client_user=EntryUser

capture_window()
{
    capture_name=$1
    if test -z "${XPILOT_E2E_SCREENSHOT_DIR:-}"; then
        return 0
    fi
    capture_variant=${XPILOT_TEST_PREFIX##*/}
    capture_xwd="$runtime_dir/$capture_name.xwd"
    capture_png="$XPILOT_E2E_SCREENSHOT_DIR/$capture_variant-$capture_name.png"
    mkdir -p -- "$XPILOT_E2E_SCREENSHOT_DIR"
    xwd -silent -id "$window_id" -out "$capture_xwd" \
        || fail "could not capture the $capture_name window"
    ffmpeg -hide_banner -loglevel error -y -i "$capture_xwd" \
        "$capture_png" \
        || fail "could not encode the $capture_name screenshot"
}

cleanup()
{
    cleanup_deadline=$(($(date +%s) + 5))
    for process_id in $multiple_client_pids "$stalled_pid" "$client_pid" "$tcp_proxy_pid" "$meta_pid" "$meta_fixture_pid" \
        "$meta_report_fixture_pid" "$server_pid"; do
        if test -n "$process_id" && kill -0 "$process_id" 2>/dev/null; then
            kill -TERM "$process_id" 2>/dev/null || true
        fi
    done
    while :; do
        cleanup_running=0
        for process_id in $multiple_client_pids "$stalled_pid" "$client_pid" "$tcp_proxy_pid" "$meta_pid" "$meta_fixture_pid" \
            "$meta_report_fixture_pid" "$server_pid"; do
            if test -n "$process_id" \
                && kill -0 "$process_id" 2>/dev/null; then
                cleanup_running=1
            fi
        done
        if test "$cleanup_running" -eq 0; then
            break
        fi
        if test "$(date +%s)" -ge "$cleanup_deadline"; then
            for process_id in $multiple_client_pids "$stalled_pid" "$client_pid" "$tcp_proxy_pid" "$meta_pid" \
                "$meta_fixture_pid" "$meta_report_fixture_pid" \
                "$server_pid"; do
                if test -n "$process_id" \
                    && kill -0 "$process_id" 2>/dev/null; then
                    kill -KILL "$process_id" 2>/dev/null || true
                fi
            done
            break
        fi
        sleep 0.1
    done
    for process_id in $multiple_client_pids "$stalled_pid" "$client_pid" "$tcp_proxy_pid" "$meta_pid" "$meta_fixture_pid" \
        "$meta_report_fixture_pid" "$server_pid"; do
        if test -n "$process_id"; then
            wait "$process_id" 2>/dev/null || true
        fi
    done
    case "$runtime_dir" in
        "${TMPDIR:-/tmp}"/xpilot-sdl3-e2e.*)
            rm -rf -- "$runtime_dir"
            ;;
    esac
}

dump_logs()
{
    if test -n "$game_server_log" && test -f "$game_server_log"; then
        echo "===== $game_server_log =====" >&2
        sed -n '1,240p' "$game_server_log" >&2
    fi
    for log_file in "$runtime_dir"/*.log; do
        if test -f "$log_file" && test "$log_file" != "$game_server_log"; then
            echo "===== $log_file =====" >&2
            sed -n '1,240p' "$log_file" >&2
        fi
    done
}

fail()
{
    echo "E2E failure: $*" >&2
    dump_logs
    exit 1
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

core_context_logged()
{
    grep -Eq '^OpenGL context: .*, profile=core, attributes=[0-9]+\.[0-9]+$' \
        "$1" 2>/dev/null
}

meta_initialized()
{
    grep -q "SDL_ttf initialized" "$runtime_dir/meta.log" 2>/dev/null \
        && core_context_logged "$runtime_dir/meta.log"
}

meta_ui_ready()
{
    if ! kill -0 "$meta_pid" 2>/dev/null; then
        fail "client stopped before the metaserver UI was drawn"
    fi
    grep -q '^Metaserver UI ready: background=semantic, buttons=3/3, draws=4$' \
        "$runtime_dir/meta.log" 2>/dev/null
}

sdl_local_browser_ready()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "SDL client stopped before the LAN server browser was drawn"
    fi
    grep -q '^Metaserver UI ready: background=semantic, buttons=3/3, draws=4$' \
	"$game_client_log" 2>/dev/null
}

meta_fixture_ready()
{
    if ! kill -0 "$meta_fixture_pid" 2>/dev/null; then
        fail "local metaserver fixture stopped before listening"
    fi
    test -s "$runtime_dir/meta-fixture.port"
}

meta_fixture_served()
{
    test -s "$runtime_dir/meta-fixture.served"
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

server_ready()
{
    if ! kill -0 "$server_pid" 2>/dev/null; then
	fail "server stopped before becoming ready"
    fi
    grep -q "Server runs at" "$game_server_log" 2>/dev/null
}

client_accepted()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "client stopped before joining the server"
    fi
    grep -q "Welcome .*$game_client_name" "$game_server_log" 2>/dev/null
}

client_transport_banner_reported()
{
    grep -Fq "*** Connected to 127.0.0.1 "\
"[Contact/Lobby: $expected_contact_transport, "\
"Gameplay: $expected_gameplay_transport]" "$game_client_log" 2>/dev/null
}

client_entry_reported()
{
    grep -Fq "$game_client_name ($game_client_user) has entered" \
	"$game_client_log" 2>/dev/null
}

client_departed()
{
    grep -q "Goodbye .*$game_client_name" "$game_server_log" 2>/dev/null
}

game_frame_ready()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "client stopped before presenting a semantic game frame"
    fi
    grep -q '^Game frame ready: semantic=ok, presented=1$' \
	"$game_client_log" 2>/dev/null
}

runtime_logs_have_no_gl_errors()
{
    if grep -q 'OpenGL error at ' "$runtime_dir"/*.log 2>/dev/null; then
        return 1
    fi
    return 0
}

find_game_window()
{
    window_id=
    for candidate_id in $(xdotool search --onlyvisible --name '^XPilot Infinity ' \
        2>/dev/null || true); do
        candidate_pid=$(xdotool getwindowpid "$candidate_id" 2>/dev/null \
            || true)
        if test "$candidate_pid" = "$window_owner_pid"; then
            window_id=$candidate_id
            return 0
        fi
    done
    return 1
}

find_connection_failure_window()
{
    # SDL's X11 message box runs in a short-lived child process, so its
    # _NET_WM_PID intentionally differs from the waiting client process.
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

find_x11_game_window()
{
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "X11 client stopped before its game window became visible"
    fi
    window_id=$(xdotool search --onlyvisible --name '^XPilot Infinity ' \
	2>/dev/null | tail -n 1 || true)
    test -n "$window_id"
}

find_x11_first_local_join_button()
{
    join_window_id=$(LC_ALL=C xwininfo -tree -id "$window_id" \
	2>/dev/null | awk '
$1 ~ /^0x[0-9a-f]+$/ && $6 ~ /^[0-9]+x[0-9]+\+/ {
    split($6, geometry, /[x+]/)
    if (geometry[2] == 23 && geometry[4] > 10 &&
        (row_y == 0 || geometry[4] < row_y ||
         (geometry[4] == row_y && geometry[3] > right_x))) {
        row_y = geometry[4]
        right_x = geometry[3]
        candidate = $1
    }
}
END {
    if (candidate != "") print candidate
}')
    test -n "$join_window_id"
}

game_window_transport_visible()
{
    game_window_title=$(xdotool getwindowname "$window_id" 2>/dev/null \
	|| true)
    test "$game_window_title" = \
	"$server_title - 127.0.0.1 "\
"[Gameplay: $expected_gameplay_transport]"
}

x11_local_game_window_transport_visible()
{
    game_window_title=$(xdotool getwindowname "$window_id" 2>/dev/null \
	|| true)
    case "$game_window_title" in
	"$server_title - "?*"[Gameplay: $expected_gameplay_transport]")
	    return 0
	    ;;
    esac
    return 1
}

quit_game_client()
{
    quit_case=$1
    window_finder=${2:-find_game_window}
    quit_deadline=$(($(date +%s) + 20))
    quit_attempted=false
    while kill -0 "$client_pid" 2>/dev/null \
	&& test "$(date +%s)" -lt "$quit_deadline"; do
	if "$window_finder"; then
	    quit_attempted=true
	    # Keep Escape and its confirmation far enough apart for SDL to
	    # process the confirmation state, then retry if X11 drops an event.
	    xdotool key --clearmodifiers --delay 150 \
		--window "$window_id" Escape y >/dev/null 2>&1 \
		|| true
	fi
	sleep 0.25
    done
    $quit_attempted \
	|| fail "could not request a graceful client quit for $quit_case"
    if kill -0 "$client_pid" 2>/dev/null; then
	fail "$quit_case client did not stop after graceful quit requests"
    fi
}

x11_game_window_absent()
{
    ! xdotool search --name '^XPilot Infinity ' >/dev/null 2>&1
}

process_window_absent()
{
    absent_pid=$1
    for candidate_id in $(xdotool search --name '^XPilot Infinity ' 2>/dev/null \
        || true); do
        candidate_pid=$(xdotool getwindowpid "$candidate_id" 2>/dev/null \
            || true)
        if test "$candidate_pid" = "$absent_pid"; then
            return 1
        fi
    done
    return 0
}

process_has_connected_inet_socket()
{
    node -e '
const fs = require("fs");
const pid = process.argv[1];
const protocol = process.argv[2];
const socketInodes = new Set();
for (const fd of fs.readdirSync(`/proc/${pid}/fd`)) {
  try {
    const target = fs.readlinkSync(`/proc/${pid}/fd/${fd}`);
    const match = /^socket:\[([0-9]+)\]$/.exec(target);
    if (match) socketInodes.add(match[1]);
  } catch {
    // File descriptors may disappear while the process is running.
  }
}
const connected = fs.readFileSync(`/proc/net/${protocol}`, "utf8")
  .trim()
  .split("\n")
  .slice(1)
  .some((line) => {
    const fields = line.trim().split(/\s+/);
    return socketInodes.has(fields[9])
      && fields[2] !== "00000000:0000"
      && (protocol === "udp" || fields[3] === "01");
  });
process.exit(connected ? 0 : 1);
' "$1" "$2"
}

process_has_connected_tcp_remote_port()
{
    node -e '
const fs = require("fs");
const pid = process.argv[1];
const expectedPort = Number(process.argv[2]);
const socketInodes = new Set();
for (const fd of fs.readdirSync(`/proc/${pid}/fd`)) {
  try {
    const target = fs.readlinkSync(`/proc/${pid}/fd/${fd}`);
    const match = /^socket:\[([0-9]+)\]$/.exec(target);
    if (match) socketInodes.add(match[1]);
  } catch {
    // File descriptors may disappear while the process is running.
  }
}
const connected = fs.readFileSync("/proc/net/tcp", "utf8")
  .trim()
  .split("\n")
  .slice(1)
  .some((line) => {
    const fields = line.trim().split(/\s+/);
    const remotePort = Number.parseInt(fields[2].split(":")[1], 16);
    return socketInodes.has(fields[9])
      && fields[3] === "01"
      && remotePort === expectedPort;
  });
process.exit(connected ? 0 : 1);
' "$1" "$2"
}

window_resized()
{
    xdotool getwindowgeometry --shell "$window_id" 2>/dev/null \
	| grep -q '^WIDTH=900$'
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

stop_local_server()
{
    kill -TERM "$server_pid" 2>/dev/null || true
    wait_until "server shutdown" 10 process_stopped "$server_pid"
    wait "$server_pid" 2>/dev/null || true
    server_pid=
}

run_invalid_target_rejection()
{
    invalid_target_log="$runtime_dir/client-invalid-target.log"

    if "$client" -join 'tls://invalid.example' \
	>"$invalid_target_log" 2>&1; then
	fail "unsupported target scheme was accepted"
    fi
    grep -Fq "Invalid server target 'tls://invalid.example'" \
	"$invalid_target_log" \
	|| fail "invalid target diagnostic did not identify the input"
    grep -Fq 'unsupported scheme; expected ws://, tcp://, or udp://' \
	"$invalid_target_log" \
	|| fail "invalid target diagnostic did not explain the scheme"
}

run_server_transport_option_help()
{
    transport_help_log="$runtime_dir/server-transport-help.log"

    "$server" -help >"$transport_help_log" 2>&1 || true
    grep -Fq -- '-tcp' "$transport_help_log" \
	|| fail "server help did not list -tcp"
    grep -Fq -- '-udp' "$transport_help_log" \
	|| fail "server help did not list -udp"
    grep -Fq -- '-websocket' "$transport_help_log" \
	|| fail "server help did not list -websocket"
    grep -Fq -- '-transport <udp|tcp|websocket>' "$transport_help_log" \
	|| fail "server help did not list -transport"
}

run_server_default_fps()
{
    default_fps_dump_log="$runtime_dir/server-default-fps-dump.log"
    "$server" -dump >"$default_fps_dump_log" 2>&1 || true
    grep -Eq '^framesPerSecond:[[:space:]]+60$' "$default_fps_dump_log" \
	|| fail "server option dump did not report the default FPS as 60"

    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-default-fps.log"
    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	-serverHost 127.0.0.1 >"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "default-FPS server readiness" 20 server_ready
    grep -Fq 'Server runs at 60 frames per second' "$game_server_log" \
	|| fail "server did not run at the default FPS of 60"
    stop_local_server
}

run_contact_target_failover()
{
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-contact-target-failover.log"
    probe_log="$runtime_dir/contact-target-failover.log"
    list_log="$runtime_dir/client-contact-target-list.log"

    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	-contactTransport tcp -gameTransport tcp -udp \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "contact target failover server readiness" 20 server_ready

    "$contact_target_probe" "tcp://127.0.0.1:$port" \
	"udp://127.0.0.1:$port" >"$probe_log" 2>&1 \
	|| fail "TCP target failure did not continue to the UDP target"
    test "$(grep -Fc 'Contacting server 127.0.0.1.' "$probe_log")" -ge 2 \
	|| fail "contact target probe did not attempt the endpoints"
    grep -Fq '[Contact/Lobby: UDP, Gameplay: UDP]' "$probe_log" \
	|| fail "contact target probe did not establish the UDP endpoint"

    "$client" -list "tcp://127.0.0.1:$port" \
	"udp://127.0.0.1:$port" >"$list_log" 2>&1 \
	|| fail "server listing did not preserve a contacted fallback result"
    grep -Fq 'TRANSPORTS.......: UDP -> UDP' "$list_log" \
	|| fail "server listing did not report the responding UDP endpoint"
    if grep -Fq 'ERROR: Connection failed:' "$list_log"; then
	fail "server listing response was reported as a connection failure"
    fi
    if xdotool search --onlyvisible \
	--name '^XPilot Infinity - Connection failed$' >/dev/null 2>&1; then
	fail "server listing displayed a connection failure dialog"
    fi
    stop_local_server
}

run_interactive_prompt_case()
{
    prompt_transport=$1
    prompt_scheme=$2
    case "$prompt_transport" in
    udp) prompt_display=UDP ;;
    tcp) prompt_display=TCP ;;
    websocket) prompt_display=WebSocket ;;
    *) fail "unsupported interactive prompt transport: $prompt_transport" ;;
    esac

    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-interactive-$prompt_transport.log"
    prompt_log="$runtime_dir/client-interactive-$prompt_transport.log"
    prompt_input="$runtime_dir/client-interactive-$prompt_transport.input"

    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	-transport "$prompt_transport" >"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$prompt_transport interactive server readiness" 20 \
	server_ready

    printf 'S\nJ\n' >"$prompt_input"
    "$contact_target_probe" --interactive \
	"$prompt_scheme://127.0.0.1:$port" \
	<"$prompt_input" >"$prompt_log" 2>&1 \
	|| fail "$prompt_transport interactive status then join failed"

    test "$(grep -Fc 'Enter command>' "$prompt_log")" -ge 2 \
	|| fail "$prompt_transport interaction did not return to the prompt"
    if test "$prompt_transport" = udp; then
	grep -Fq '*** Login allowed.' "$prompt_log" \
	    || fail "UDP interactive join was not admitted"
    else
	grep -Fq "[Contact/Lobby: $prompt_display, "\
"Gameplay: $prompt_display]" "$prompt_log" \
	    || fail "$prompt_transport interactive join omitted its transports"
    fi
    wait_until "$prompt_transport interactive status request" 5 \
	grep -q 'asked for info about current game' "$game_server_log"
    stop_local_server
}

run_connection_failure_notification()
{
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-connection-failure.log"
    game_client_log="$runtime_dir/client-connection-failure.log"

    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	-transport udp >"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "UDP-only failure fixture readiness" 20 server_ready

    text_failure_log="$runtime_dir/client-text-connection-failure.log"
    if "$client" -text "tcp://127.0.0.1:$port" \
	>"$text_failure_log" 2>&1; then
	fail "text-mode connection failure returned a successful exit status"
    fi
    grep -Fq "Could not contact 127.0.0.1:$port." "$text_failure_log" \
	|| fail "text-mode failure did not identify the endpoint"
    if xdotool search --onlyvisible \
	--name '^XPilot Infinity - Connection failed$' >/dev/null 2>&1; then
	fail "text-mode connection failure displayed a dialog"
    fi

    "$client" "tcp://127.0.0.1:$port" >"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "connection failure dialog" 30 \
	connection_failure_window_visible

    kill -0 "$client_pid" 2>/dev/null \
	|| fail "client exited while the connection failure dialog was visible"
    grep -Fq "Could not contact 127.0.0.1:$port." "$game_client_log" \
	|| fail "final connection failure did not identify the endpoint"
    grep -Fq 'Contact/Lobby: TCP' "$game_client_log" \
	|| fail "final connection failure omitted the contact transport"
    grep -Fq 'Gameplay: TCP' "$game_client_log" \
	|| fail "final connection failure omitted the gameplay transport"

    failure_window_count=$(xdotool search --onlyvisible \
	--name '^XPilot Infinity - Connection failed$' 2>/dev/null \
	| wc -l)
    test "$failure_window_count" -eq 1 \
	|| fail "expected one final connection failure dialog"

    xdotool key --clearmodifiers --window "$window_id" Return \
	>/dev/null 2>&1 \
	|| fail "could not dismiss the connection failure dialog"
    wait_until "client exit after failure acknowledgement" 10 \
	process_stopped "$client_pid"
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    test "$client_status" -ne 0 \
	|| fail "connection failure returned a successful exit status"

    window_id=
    stop_local_server
}

run_recording_playback()
{
    playback_case=$1
    playback_transport=$2
    playback_recording=$3
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-$playback_case-playback.log"

    test -s "$playback_recording" \
	|| fail "$playback_case recording was not written"
    if test "$playback_transport" = default; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -recordFileName "$playback_recording" -recordMode 2 \
	    >"$game_server_log" 2>&1 &
    else
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -gameTransport "$playback_transport" \
	    -recordFileName "$playback_recording" -recordMode 2 \
	    >"$game_server_log" 2>&1 &
    fi
    server_pid=$!
    wait_until "$playback_case playback startup" 20 \
	grep -q "Server runs at" "$game_server_log"
    wait_until "$playback_case recorded join" 20 \
	grep -q "Welcome .*$game_client_name" "$game_server_log"
    wait_until "$playback_case recorded quit" 20 \
	grep -q "Goodbye .*$game_client_name" "$game_server_log"
    stop_local_server
}

run_gameplay_case()
{
    game_case=$1
    game_transport=$2
    game_capture=$3
    contact_transport=$4

    case "$game_transport" in
    default|udp) expected_gameplay_transport=UDP ;;
    tcp) expected_gameplay_transport=TCP ;;
    websocket) expected_gameplay_transport=WebSocket ;;
    esac
    case "$contact_transport" in
    default|udp) expected_contact_transport=UDP ;;
    tcp) expected_contact_transport=TCP ;;
    websocket) expected_contact_transport=WebSocket ;;
    esac
    port=$(reserve_contact_port)
    game_recording="$runtime_dir/server-$game_case.xpr"
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"
    case "$game_case" in
    tcp)
	game_client_name=SDL3TCP
	;;
    tcp-contact)
	game_client_name=SDL3TCPContact
	;;
    tcp-contact-udp-game)
	game_client_name=SDL3TCPUDP
	;;
    websocket)
	game_client_name=SDL3WebSocket
	;;
    udp-default)
	game_client_name=SDL3UDPDefault
	;;
    udp-explicit)
	game_client_name=SDL3UDPOption
	;;
    esac

    if test "$game_case" = udp-explicit; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -contactTransport tcp -gameTransport tcp -transport udp \
	    -recordFileName "$game_recording" -recordMode 1 \
	    >"$game_server_log" 2>&1 &
    elif test "$game_case" = tcp-contact; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -transport tcp \
	    >"$game_server_log" 2>&1 &
    elif test "$game_case" = tcp-contact-udp-game; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -tcp -gameTransport udp \
	    -recordFileName "$game_recording" -recordMode 1 \
	    >"$game_server_log" 2>&1 &
    elif test "$game_case" = websocket; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -websocket >"$game_server_log" 2>&1 &
    elif test "$game_transport" = default \
	&& test "$contact_transport" = default; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -recordFileName "$game_recording" -recordMode 1 \
	    >"$game_server_log" 2>&1 &
    elif test "$contact_transport" = default; then
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -gameTransport "$game_transport" \
	    -recordFileName "$game_recording" -recordMode 1 \
	    >"$game_server_log" 2>&1 &
    else
	"$server" -map "$map" -port "$port" -noQuit +reportMeta \
	    -gameTransport "$game_transport" \
	    -contactTransport "$contact_transport" \
	    -recordFileName "$game_recording" -recordMode 1 \
	    >"$game_server_log" 2>&1 &
    fi
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    if test "$game_case" = udp-explicit; then
	"$client" -geometry 800x600 -join \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 \
	    -contactTransport tcp -gameTransport tcp \
	    "udp://127.0.0.1:$port" >"$game_client_log" 2>&1 &
    elif test "$game_case" = tcp-contact; then
	"$client" -geometry 800x600 -join \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 \
	    "tcp://127.0.0.1:$port" >"$game_client_log" 2>&1 &
    elif test "$game_case" = websocket; then
	"$client" -geometry 800x600 -join \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 \
	    "ws://127.0.0.1:$port" >"$game_client_log" 2>&1 &
    elif test "$game_transport" = default \
	&& test "$contact_transport" = default; then
	"$client" -geometry 800x600 -join -port "$port" \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 \
	    127.0.0.1 >"$game_client_log" 2>&1 &
    elif test "$contact_transport" = default; then
	"$client" -geometry 800x600 -join -port "$port" \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 -gameTransport "$game_transport" \
	    127.0.0.1 >"$game_client_log" 2>&1 &
    else
	"$client" -geometry 800x600 -join -port "$port" \
	    -name "$game_client_name" -user "$game_client_user" \
	    -messagesToStdout 2 -gameTransport "$game_transport" \
	    -contactTransport "$contact_transport" \
	    127.0.0.1 >"$game_client_log" 2>&1 &
    fi
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case SDL game window" 20 find_game_window
    wait_until "$game_case gameplay transport window title" 10 \
	game_window_transport_visible
    wait_until "$game_case local client acceptance" 20 client_accepted
    wait_until "$game_case connection transport banner" 10 \
	client_transport_banner_reported
    if test "$game_transport" = tcp \
	|| test "$game_transport" = websocket; then
	game_socket_protocol=tcp
    else
	game_socket_protocol=udp
    fi
    wait_until "$game_case client $game_socket_protocol connection" 5 \
	process_has_connected_inet_socket "$client_pid" "$game_socket_protocol"
    wait_until "$game_case server $game_socket_protocol connection" 5 \
	process_has_connected_inet_socket "$server_pid" "$game_socket_protocol"
    if test "$game_case" = tcp-contact \
	|| test "$game_case" = websocket; then
	wait_until "$game_case gameplay on fixed contact port" 5 \
	    process_has_connected_tcp_remote_port "$client_pid" "$port"
    fi
    wait_until "$game_case game core OpenGL context diagnostics" 10 \
	core_context_logged "$game_client_log"
    wait_until "$game_case game text renderers" 10 \
	grep -q '^Font text renderers ready: game=renderer map=renderer$' \
	    "$game_client_log"
    wait_until "$game_case semantic game frame presentation" 20 \
	game_frame_ready
    test -r "$runtime_dir/textures/ndh-1.3/bakedmud.pnm" \
	|| fail "bundled map data was not extracted for $game_case"
    test ! -e "$XPILOT_TEST_PKGDATADIR/textures/ndh-1.3.xpd" \
	|| fail "bundled map data modified the installed texture directory"

    xdotool windowsize "$window_id" 900 700 >/dev/null 2>&1 \
	|| fail "could not request an SDL window resize for $game_case"
    wait_until "$game_case SDL window resize" 10 window_resized
    xdotool keydown --window "$window_id" Shift_L >/dev/null 2>&1 \
	|| fail "could not send key-down event for $game_case"
    xdotool keyup --window "$window_id" Shift_L >/dev/null 2>&1 \
	|| fail "could not send key-up event for $game_case"
    xdotool key --window "$window_id" Up Return >/dev/null 2>&1 \
	|| fail "could not send gameplay key events for $game_case"
    xdotool key --window "$window_id" m >/dev/null 2>&1 \
	|| fail "could not open the console for $game_case"
    xdotool type --window "$window_id" --delay 10 "$game_client_name" \
	>/dev/null 2>&1 \
	|| fail "could not enter console text for $game_case"
    xdotool key --window "$window_id" Return >/dev/null 2>&1 \
	|| fail "could not submit console text for $game_case"
    if ! kill -0 "$client_pid" 2>/dev/null; then
	fail "client stopped after resize/input events for $game_case"
    fi
    if test "$game_capture" = yes; then
	capture_window game
    fi

    quit_game_client "$game_case"
    finished_client_pid=$client_pid
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    if test "$client_status" -ne 0; then
	fail "$game_case client returned status $client_status"
    fi
    client_entry_reported \
	|| fail "$game_case client did not report the first-player entry announcement"
    wait_until "$game_case game window teardown" 5 process_window_absent \
	"$finished_client_pid"
    wait_until "$game_case server-side client departure" 5 client_departed
    if test "$game_case" = websocket; then
	game_client_name=SDL3WSRejoin
	game_client_log="$runtime_dir/client-$game_case-rejoin.log"
	"$client" -geometry 800x600 -join -texturedWalls no \
	    -name "$game_client_name" \
	    "ws://127.0.0.1:$port" >"$game_client_log" 2>&1 &
	client_pid=$!
	window_owner_pid=$client_pid
	wait_until "$game_case rejoin SDL game window" 20 find_game_window
	wait_until "$game_case rejoin gameplay transport window title" 10 \
	    game_window_transport_visible
	wait_until "$game_case rejoin local client acceptance" 20 \
	    client_accepted
	wait_until "$game_case rejoin connection transport banner" 10 \
	    client_transport_banner_reported
	wait_until "$game_case rejoin semantic game frame presentation" 20 \
	    game_frame_ready

	quit_game_client "$game_case rejoin"
	finished_client_pid=$client_pid
	set +e
	wait "$client_pid"
	client_status=$?
	set -e
	client_pid=
	window_id=
	if test "$client_status" -ne 0; then
	    fail "$game_case rejoin client returned status $client_status"
	fi
	wait_until "$game_case rejoin game window teardown" 5 \
	    process_window_absent "$finished_client_pid"
	wait_until "$game_case rejoin server-side client departure" 5 \
	    client_departed
    fi
    stop_local_server
    if test "$game_case" != tcp-contact \
	&& test "$game_case" != websocket; then
	run_recording_playback "$game_case" "$game_transport" "$game_recording"
    fi
}

run_tcp_reconnection_case()
{
    game_case=tcp-reconnect
    port=$(reserve_contact_port)
    proxy_host=127.0.0.2
    proxy_state="$runtime_dir/tcp-reconnect-proxy.state"
    proxy_trigger="$runtime_dir/tcp-reconnect.trigger"
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"
    game_client_name=SDL3TCPResume

    "$server" -map "$map" -port "$port" +reportMeta \
	-serverHost 127.0.0.1 -gameTransport tcp \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    node "$(dirname "$0")/tcp-reconnect-proxy.mjs" \
	127.0.0.1 "$proxy_host" "$port" "$proxy_trigger" "$proxy_state" \
	>"$runtime_dir/tcp-reconnect-proxy.log" 2>&1 &
    tcp_proxy_pid=$!
    wait_until "$game_case proxy readiness" 10 \
	grep -q '^contact-ready$' "$proxy_state"

    "$client" -geometry 800x600 -join -name "$game_client_name" \
	-port "$port" -contactTransport udp -gameTransport tcp \
	"$proxy_host" >"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case SDL game window" 20 find_game_window
    wait_until "$game_case local client acceptance" 20 client_accepted
    wait_until "$game_case semantic game frame presentation" 20 \
	game_frame_ready

    touch "$proxy_trigger"
    wait_until "$game_case forced transport loss" 10 \
	grep -q '^dropped$' "$proxy_state"
    wait_until "$game_case server gameplay suspension" 10 \
	grep -q "TCP gameplay connection suspended.*$game_client_name" \
	    "$game_server_log"
    kill -0 "$server_pid" 2>/dev/null \
	|| fail "$game_case server exited during the reconnection grace period"
    kill -0 "$client_pid" 2>/dev/null \
	|| fail "$game_case client exited instead of reconnecting"

    wait_until "$game_case replacement TCP stream" 15 \
	grep -q '^resumed$' "$proxy_state"
    wait_until "$game_case server session resumption" 15 \
	grep -q "TCP gameplay connection resumed.*$game_client_name" \
	    "$game_server_log"
    test "$(grep -c "Welcome .*$game_client_name" "$game_server_log")" -eq 1 \
	|| fail "$game_case created a second player session"
    if grep -q "Goodbye .*$game_client_name" "$game_server_log"; then
	fail "$game_case removed the player before graceful quit"
    fi

    quit_game_client "$game_case"
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    test "$client_status" -eq 0 \
	|| fail "$game_case client returned status $client_status"
    wait_until "$game_case graceful server shutdown" 10 \
	process_stopped "$server_pid"
    wait "$server_pid" 2>/dev/null || true
    server_pid=
    grep -q "Goodbye .*$game_client_name.*client quit" "$game_server_log" \
	|| fail "$game_case graceful quit was not handled immediately"

    kill -TERM "$tcp_proxy_pid" 2>/dev/null || true
    wait "$tcp_proxy_pid" 2>/dev/null || true
    tcp_proxy_pid=
}

run_multiple_protocol_game()
{
    udp_port=$(reserve_contact_port)
    tcp_port=$(reserve_contact_port)
    while test "$tcp_port" = "$udp_port"; do tcp_port=$(reserve_contact_port); done
    ws_port=$(reserve_contact_port)
    while test "$ws_port" = "$udp_port" || test "$ws_port" = "$tcp_port"; do
        ws_port=$(reserve_contact_port)
    done
    game_server_log="$runtime_dir/server-multiple.log"
    proxy_state="$runtime_dir/multiple-proxy.state"
    proxy_trigger="$runtime_dir/multiple.trigger"
    node -e '
      const fs = require("node:fs");
      const socket = require("node:dgram").createSocket("udp4");
      const dir = process.argv[1];
      socket.on("message", (packet, peer) => {
        const text = packet.toString().replace(/\0/g, "");
        const kind = text.includes("\nremove") ? "removed" : "update";
        fs.writeFileSync(dir + "/multiple-meta-" + peer.port + "." + kind, text);
      });
      socket.bind(0, "127.0.0.1", () => fs.writeFileSync(
        dir + "/multiple-meta.port", String(socket.address().port)));
    ' "$runtime_dir" >"$runtime_dir/multiple-meta.log" 2>&1 &
    meta_report_fixture_pid=$!
    wait_until "mixed metaserver fixture" 10 test -s "$runtime_dir/multiple-meta.port"
    XPILOT_META_REPORT_HOST=127.0.0.1 XPILOT_META_REPORT_HOST_TWO=127.0.0.1 \
    XPILOT_META_REPORT_PORT=$(cat "$runtime_dir/multiple-meta.port") \
    "$server" -map "$map" -noQuit -reportMeta -serverHost 127.0.0.1 \
        -listen "udp:$udp_port,tcp:$tcp_port,websocket:$ws_port" \
        >"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "multiple protocol server readiness" 20 server_ready
    node "$(dirname "$0")/session-reconnect-proxy.mjs" \
        127.0.0.1 127.0.0.2 "$tcp_port" "$proxy_trigger" "$proxy_state" tcp "$ws_port" \
        >"$runtime_dir/multiple-proxy.log" 2>&1 &
    tcp_proxy_pid=$!
    wait_until "multiple protocol proxy readiness" 10 grep -q '^ready$' "$proxy_state"

    # Leave an HTTP handshake unfinished while real clients exchange gameplay.
    node -e 'const net = require("node:net");
      const socket = net.createConnection(Number(process.argv[1]), "127.0.0.1",
        () => { socket.write("G"); console.log("ready"); });
      socket.on("error", () => {});' "$ws_port" \
        >"$runtime_dir/multiple-stalled.log" 2>&1 &
    stalled_pid=$!
    wait_until "unfinished WebSocket handshake" 10 \
        grep -q ready "$runtime_dir/multiple-stalled.log"

    for mode in udp tcp ws; do
        case "$mode" in
            udp) target="udp://127.0.0.1:$udp_port"; game_client_name=MixedUDP ;;
            tcp) target="tcp://127.0.0.2:$tcp_port"; game_client_name=MixedTCP ;;
            ws) target="ws://127.0.0.1:$ws_port"; game_client_name=MixedWS ;;
        esac
        game_client_log="$runtime_dir/client-multiple-$mode.log"
        stdbuf -oL "$client" -geometry 800x600 -join -name "$game_client_name" \
            -messagesToStdout 2 "$target" >"$game_client_log" 2>&1 &
        client_pid=$!
        multiple_client_pids="$multiple_client_pids $client_pid"
        echo "$client_pid" >"$runtime_dir/multiple-$mode.pid"
        window_owner_pid=$client_pid
        wait_until "$mode mixed game window" 20 find_game_window
        wait_until "$mode mixed game admission" 20 client_accepted
        wait_until "$mode mixed semantic game frame" 20 game_frame_ready
    done

    for report_port in "$udp_port" "$tcp_port" "$ws_port"; do
        wait_until "shared three-player advertisement on $report_port" 10 \
            grep -qx 'add users 3' "$runtime_dir/multiple-meta-$report_port.update"
    done

    for mode in udp tcp ws; do
        window_owner_pid=$(cat "$runtime_dir/multiple-$mode.pid")
        wait_until "$mode mixed game window" 10 find_game_window
        xdotool windowfocus --sync "$window_id"
        xdotool key --window "$window_id" m
        xdotool type --window "$window_id" --delay 10 "mixed-world-$mode"
        xdotool key --window "$window_id" Return
        for receiver in udp tcp ws; do
            wait_until "$receiver receives $mode gameplay message" 10 \
                grep -q "mixed-world-$mode" "$runtime_dir/client-multiple-$receiver.log"
        done
    done

    touch "$proxy_trigger"
    wait_until "mixed TCP suspension" 10 \
        grep -q 'tcp gameplay session suspended.*MixedTCP' "$game_server_log"
    wait_until "mixed TCP invalid token rejection" 10 \
        grep -q '^invalid-rejected$' "$proxy_state"
    wait_until "TCP token rejected on WebSocket listener" 10 \
        grep -q '^wrong-protocol-rejected$' "$proxy_state"
    wait_until "mixed TCP resumption" 15 \
        grep -q 'tcp gameplay session resumed.*MixedTCP' "$game_server_log"
    test "$(grep -c 'Welcome .*MixedTCP' "$game_server_log")" -eq 1 \
        || fail "mixed TCP resumption created a second player"
    window_owner_pid=$(cat "$runtime_dir/multiple-tcp.pid")
    wait_until "resumed TCP window" 10 find_game_window
    xdotool windowfocus --sync "$window_id"
    xdotool key --window "$window_id" m
    xdotool type --window "$window_id" --delay 10 mixed-resumed
    xdotool key --window "$window_id" Return
    for receiver in udp tcp ws; do
        wait_until "$receiver receives resumed TCP gameplay" 10 \
            grep -q mixed-resumed "$runtime_dir/client-multiple-$receiver.log"
    done

    client_pid=$window_owner_pid
    quit_game_client "mixed TCP"
    wait "$client_pid" || fail "mixed TCP quit failed"
    wait_until "mixed TCP departure" 10 \
        grep -q 'Goodbye .*MixedTCP.*client quit' "$game_server_log"
    client_pid=
    multiple_client_pids="$(cat "$runtime_dir/multiple-udp.pid") $(cat "$runtime_dir/multiple-ws.pid")"
    window_owner_pid=$(cat "$runtime_dir/multiple-udp.pid")
    wait_until "remaining UDP window" 10 find_game_window
    xdotool windowfocus --sync "$window_id"
    xdotool key --window "$window_id" m
    xdotool type --window "$window_id" --delay 10 mixed-after-tcp-quit
    xdotool key --window "$window_id" Return
    wait_until "WebSocket gameplay after TCP quit" 10 \
        grep -q mixed-after-tcp-quit "$runtime_dir/client-multiple-ws.log"

    for report_port in "$udp_port" "$tcp_port" "$ws_port"; do
        wait_until "shared two-player advertisement on $report_port" 10 \
            grep -qx 'add users 2' "$runtime_dir/multiple-meta-$report_port.update"
    done

    for mode in udp ws; do
        client_pid=$(cat "$runtime_dir/multiple-$mode.pid")
        window_owner_pid=$client_pid
        quit_game_client "mixed $mode"
        wait "$client_pid" || fail "mixed $mode quit failed"
        client_pid=
    done
    multiple_client_pids=
    for process_id in "$stalled_pid" "$tcp_proxy_pid" "$server_pid"; do
        kill -TERM "$process_id" 2>/dev/null || true
        wait "$process_id" 2>/dev/null || true
    done
    for report_port in "$udp_port" "$tcp_port" "$ws_port"; do
        wait_until "mixed server removal on $report_port" 10 \
            test -s "$runtime_dir/multiple-meta-$report_port.removed"
    done
    kill -TERM "$meta_report_fixture_pid" 2>/dev/null || true
    wait "$meta_report_fixture_pid" 2>/dev/null || true
    meta_report_fixture_pid=
    stalled_pid=
    tcp_proxy_pid=
    server_pid=
}

run_fixed_session_reconnection_case()
{
    session_transport=$1
    case "$session_transport" in
    tcp)
	game_case=fixed-tcp-reconnect
	server_transport_option=-tcp
	target_scheme=tcp
	game_client_name=FixedTCPResume
	;;
    websocket)
	game_case=websocket-reconnect
	server_transport_option=-websocket
	target_scheme=ws
	game_client_name=WebSockResume
	;;
    *)
	fail "unsupported fixed-session reconnect transport: $session_transport"
	;;
    esac
    port=$(reserve_contact_port)
    proxy_host=127.0.0.2
    proxy_state="$runtime_dir/$game_case-proxy.state"
    proxy_trigger="$runtime_dir/$game_case.trigger"
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"

    "$server" -map "$map" -port "$port" +reportMeta \
	-serverHost 127.0.0.1 "$server_transport_option" \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    node "$(dirname "$0")/session-reconnect-proxy.mjs" \
	127.0.0.1 "$proxy_host" "$port" "$proxy_trigger" "$proxy_state" \
	"$session_transport" \
	>"$runtime_dir/$game_case-proxy.log" 2>&1 &
    tcp_proxy_pid=$!
    wait_until "$game_case proxy readiness" 10 \
	grep -q '^ready$' "$proxy_state"

    "$client" -geometry 800x600 -join -name "$game_client_name" \
	"$target_scheme://$proxy_host:$port" >"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case SDL game window" 20 find_game_window
    wait_until "$game_case local client acceptance" 20 client_accepted
    wait_until "$game_case semantic game frame presentation" 20 \
	game_frame_ready

    touch "$proxy_trigger"
    wait_until "$game_case forced transport loss" 10 \
	grep -q '^dropped$' "$proxy_state"
    wait_until "$game_case server session suspension" 10 \
	grep -q "$session_transport gameplay session suspended.*$game_client_name" \
	    "$game_server_log"
    kill -0 "$server_pid" 2>/dev/null \
	|| fail "$game_case server exited during the reconnection grace period"
    kill -0 "$client_pid" 2>/dev/null \
	|| fail "$game_case client exited instead of reconnecting"
    if test "$session_transport" = tcp; then
	wait_until "$game_case invalid resume rejection" 10 \
	    grep -q '^invalid-rejected$' "$proxy_state"
	kill -0 "$server_pid" 2>/dev/null \
	    || fail "$game_case invalid resume stopped the server"
	kill -0 "$client_pid" 2>/dev/null \
	    || fail "$game_case invalid resume removed the active player"
    fi

    wait_until "$game_case replacement session stream" 15 \
	grep -q '^resumed$' "$proxy_state"
    wait_until "$game_case server session resumption" 15 \
	grep -q "$session_transport gameplay session resumed.*$game_client_name" \
	    "$game_server_log"
    test "$(grep -c "Welcome .*$game_client_name" "$game_server_log")" -eq 1 \
	|| fail "$game_case created a second player session"
    if grep -q "Goodbye .*$game_client_name" "$game_server_log"; then
	fail "$game_case removed the player before graceful quit"
    fi
    xdotool key --window "$window_id" Up >/dev/null 2>&1 \
	|| fail "could not send gameplay input after $game_case resumption"
    kill -0 "$client_pid" 2>/dev/null \
	|| fail "$game_case client stopped after resuming gameplay"

    quit_game_client "$game_case"
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    test "$client_status" -eq 0 \
	|| fail "$game_case client returned status $client_status"
    wait_until "$game_case graceful server shutdown" 10 \
	process_stopped "$server_pid"
    wait "$server_pid" 2>/dev/null || true
    server_pid=
    grep -q "Goodbye .*$game_client_name.*client quit" "$game_server_log" \
	|| fail "$game_case graceful quit was not handled immediately"

    kill -TERM "$tcp_proxy_pid" 2>/dev/null || true
    wait "$tcp_proxy_pid" 2>/dev/null || true
    tcp_proxy_pid=
}

run_x11_gameplay_case()
{
    transport=$1
    case "$transport" in
    tcp)
	game_case=x11-tcp
	expected_contact_transport=TCP
	expected_gameplay_transport=TCP
	server_transport_option=-tcp
	target_scheme=tcp
	game_client_name=X11TCP
	;;
    websocket)
	game_case=x11-websocket
	expected_contact_transport=WebSocket
	expected_gameplay_transport=WebSocket
	server_transport_option=-websocket
	target_scheme=ws
	game_client_name=X11WebSocket
	;;
    *)
	fail "unsupported X11 gameplay transport: $transport"
	;;
    esac
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"

    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	"$server_transport_option" \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    "$x11_client" -geometry 800x600 -join \
	-name "$game_client_name" \
	"$target_scheme://127.0.0.1:$port" >"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case game window" 20 find_x11_game_window
    wait_until "$game_case gameplay transport window title" 10 \
	game_window_transport_visible
    wait_until "$game_case local client acceptance" 20 client_accepted

    quit_game_client "$game_case" find_x11_game_window
    finished_client_pid=$client_pid
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    if test "$client_status" -ne 0; then
	fail "$game_case client returned status $client_status"
    fi
    client_transport_banner_reported \
	|| fail "$game_case did not report the connection transports"
    wait_until "$game_case game window teardown" 5 x11_game_window_absent
    wait_until "$game_case server-side client departure" 5 client_departed
    stop_local_server
}

run_x11_local_discovery_case()
{
    game_case=x11-local-discovery
    expected_contact_transport=UDP
    expected_gameplay_transport=UDP
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"
    game_client_name=X11Local

    "$server" -map "$map" -port "$port" -noQuit +reportMeta -udp \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    "$x11_client" -geometry 800x600 -port "$port" \
	-name "$game_client_name" >"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case launcher window" 20 find_x11_game_window

    xdotool mousemove --window "$window_id" 55 60 click 1 \
	>/dev/null 2>&1 \
	|| fail "$game_case could not select local discovery"
    wait_until "$game_case local server discovery" 10 \
	grep -q 'Using protocol version' "$game_client_log"

    wait_until "$game_case local join button" 5 \
	find_x11_first_local_join_button
    xdotool mousemove --window "$join_window_id" 10 10 click 1 \
	>/dev/null 2>&1 \
	|| fail "$game_case could not select the discovered server"
    wait_until "$game_case local client acceptance" 20 client_accepted
    wait_until "$game_case gameplay transport window title" 10 \
	x11_local_game_window_transport_visible

    quit_game_client "$game_case" find_x11_game_window
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    test "$client_status" -eq 0 \
	|| fail "$game_case client returned status $client_status"
    wait_until "$game_case server-side client departure" 5 client_departed
    stop_local_server
}

run_sdl_local_discovery_case()
{
    game_case=sdl-local-discovery
    expected_contact_transport=UDP
    expected_gameplay_transport=UDP
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"
    game_client_name=SDL3Local

    "$server" -map "$map" -port "$port" -noQuit +reportMeta -udp \
	>"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    "$client" -port "$port" -name "$game_client_name" \
	-user "$game_client_user" -messagesToStdout 2 \
	>"$game_client_log" 2>&1 &
    client_pid=$!
    window_owner_pid=$client_pid
    wait_until "$game_case LAN query" 20 \
	grep -q 'Searching for an XPilot server on the local net' \
	    "$game_client_log"
    wait_until "$game_case browser UI" 30 sdl_local_browser_ready
    grep -q "Couldn't get meta list" "$game_client_log" \
	|| fail "$game_case did not continue after metaserver failure"
    if grep -q 'Searching once more' "$game_client_log"; then
	fail "$game_case performed more than one LAN discovery pass"
    fi
    wait_until "$game_case browser window" 10 find_game_window

    # The combined browser prioritizes LAN entries and selects its first row.
    # Activating that row proves the discovery result is present and joinable.
    xdotool mousemove --window "$window_id" 280 220 click 1 \
	>/dev/null 2>&1 \
	|| fail "$game_case could not activate the first LAN server row"
    wait_until "$game_case local client acceptance" 20 client_accepted
    wait_until "$game_case gameplay transport window title" 10 \
	x11_local_game_window_transport_visible
    wait_until "$game_case semantic game frame presentation" 20 \
	game_frame_ready
    grep -Fq '[Contact/Lobby: UDP, Gameplay: UDP]' "$game_client_log" \
	|| fail "$game_case did not report the LAN connection transports"

    quit_game_client "$game_case"
    finished_client_pid=$client_pid
    set +e
    wait "$client_pid"
    client_status=$?
    set -e
    client_pid=
    window_id=
    test "$client_status" -eq 0 \
	|| fail "$game_case client returned status $client_status"
    wait_until "$game_case game window teardown" 5 process_window_absent \
	"$finished_client_pid"
    wait_until "$game_case server-side client departure" 5 client_departed
    stop_local_server
}

run_transport_mismatch()
{
    server_transport=$1
    client_transport=$2
    game_case="mismatch-$server_transport-server-$client_transport-client"
    port=$(reserve_contact_port)
    game_server_log="$runtime_dir/server-$game_case.log"
    game_client_log="$runtime_dir/client-$game_case.log"
    game_client_name="SDL3Mismatch-$server_transport-$client_transport"

    "$server" -map "$map" -port "$port" -noQuit +reportMeta \
	-gameTransport "$server_transport" >"$game_server_log" 2>&1 &
    server_pid=$!
    wait_until "$game_case server readiness" 20 server_ready

    "$client" -geometry 800x600 -join -port "$port" \
	-name "$game_client_name" -gameTransport "$client_transport" \
	127.0.0.1 >"$game_client_log" 2>&1 &
    client_pid=$!
    rejected_client_pid=$client_pid
    wait_until "$game_case client rejection" 15 process_stopped "$client_pid"
    wait "$client_pid" 2>/dev/null || true
    client_pid=
    grep -q 'Gameplay transport mismatch with server' "$game_client_log" \
	|| fail "$game_case did not report the transport mismatch"
    if grep -q "Welcome .*$game_client_name" "$game_server_log"; then
	fail "$game_case advanced to gameplay"
    fi
    wait_until "$game_case window absence" 5 process_window_absent \
	"$rejected_client_pid"
    stop_local_server
}

mkdir -p -- "$runtime_dir/textures"
# The virtual display does not isolate the host's audio device.
printf 'xpilot.sound: no\nxpilot.texturePath: %s:%s\n' \
    "$runtime_dir/textures" "$XPILOT_TEST_PKGDATADIR/textures" \
    >"$runtime_dir/xpilotrc"
export XPILOTRC="$runtime_dir/xpilotrc"

node -e '
const dgram = require("node:dgram");
const fs = require("fs");
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
wait_until "local metaserver report fixture" 10 meta_report_fixture_ready

meta_report_contact_port=$(reserve_contact_port)
game_server_log="$runtime_dir/server-meta-tcp-contact.log"
XPILOT_META_REPORT_HOST=127.0.0.1 \
XPILOT_META_REPORT_HOST_TWO=127.0.0.1 \
XPILOT_META_REPORT_PORT=$(sed -n '1p' \
    "$runtime_dir/meta-report-fixture.port") \
    "$server" -map "$map" -port "$meta_report_contact_port" -noQuit \
    -reportMeta -contactTransport tcp -gameTransport udp \
    >"$game_server_log" 2>&1 &
server_pid=$!
wait_until "TCP-contact metaserver report startup" 20 server_ready
wait_until "TCP-contact metaserver transport advertisement" 15 \
    meta_tcp_transport_reported
stop_local_server
kill -TERM "$meta_report_fixture_pid" 2>/dev/null || true
wait "$meta_report_fixture_pid" 2>/dev/null || true
meta_report_fixture_pid=

node -e '
const fs = require("fs");
const net = require("net");
const portFile = process.argv[1];
const servedFile = process.argv[2];
const response = Array.from({ length: 12 }, (_, index) => [
  index % 2 === 0
    ? "4.7.3+ct=tcp+gt=udp"
    : "4.7.3+ct=udp+gt=tcp",
  `fixture${index}.local`,
  String(15000 + index),
  String(index),
  "fixture-map",
  "100x100",
  "test-author",
  "ok",
  "10",
  "20",
  "-",
  "no",
  "100",
  "0",
  "0",
  `127.0.0.${index + 1}`,
  "10",
  "0",
].join(":"))
  .join("\n") + "\n";
let responseSent = false;
const server = net.createServer((socket) => {
  fs.appendFileSync(servedFile, "served\n");
  if (responseSent) {
    socket.end();
    return;
  }
  responseSent = true;
  socket.end(response);
});
server.listen(0, "127.0.0.1", () => {
  fs.writeFileSync(portFile, String(server.address().port));
});
const stop = () => server.close(() => process.exit(0));
process.on("SIGTERM", stop);
process.on("SIGINT", stop);
' "$runtime_dir/meta-fixture.port" "$runtime_dir/meta-fixture.served" \
    >"$runtime_dir/meta-fixture.log" 2>&1 &
meta_fixture_pid=$!
wait_until "local metaserver fixture" 10 meta_fixture_ready
export XPILOT_META_HOST=127.0.0.1
export XPILOT_META_HOST_TWO=127.0.0.1
XPILOT_META_PORT=$(sed -n '1p' "$runtime_dir/meta-fixture.port")
export XPILOT_META_PORT

# With no server target the SDL client takes the graphical browser path.  LAN
# discovery is disabled here so this scenario isolates a completed metaserver
# fetch, semantic drawing, presentation, and graceful teardown.
"$client" -localDiscovery no >"$runtime_dir/meta.log" 2>&1 &
meta_pid=$!
window_owner_pid=$meta_pid
wait_until "no-argument SDL initialization" 15 meta_initialized
wait_until "metaserver text renderers" 10 \
    grep -q '^Font text renderers ready: game=renderer map=renderer$' \
        "$runtime_dir/meta.log"
wait_until "semantic metaserver UI" 20 meta_ui_ready
wait_until "local metaserver request" 5 meta_fixture_served
find_game_window || fail "metaserver window was not visible"
capture_window metaserver
kill -0 "$meta_pid" 2>/dev/null \
    || fail "metaserver client exited before Escape"
meta_deadline=$(($(date +%s) + 20))
meta_escape_sent=false
while kill -0 "$meta_pid" 2>/dev/null \
    && test "$(date +%s)" -lt "$meta_deadline"; do
    if find_game_window \
        && xdotool key --clearmodifiers --window "$window_id" Escape \
            >/dev/null 2>&1; then
        meta_escape_sent=true
    fi
    sleep 0.1
done
$meta_escape_sent || fail "could not send Escape to the metaserver UI"
kill -0 "$meta_pid" 2>/dev/null \
    && fail "metaserver UI did not close after Escape"
finished_meta_pid=$meta_pid
set +e
wait "$meta_pid"
meta_status=$?
set -e
meta_pid=
window_id=
if test "$meta_status" -ne 0; then
    fail "metaserver client returned status $meta_status"
fi
wait_until "metaserver window teardown" 5 process_window_absent \
    "$finished_meta_pid"

kill -TERM "$meta_fixture_pid" 2>/dev/null || true
wait "$meta_fixture_pid" 2>/dev/null || true
meta_fixture_pid=
run_sdl_local_discovery_case

run_invalid_target_rejection
run_server_transport_option_help
run_server_default_fps
run_contact_target_failover
run_interactive_prompt_case tcp tcp
run_interactive_prompt_case websocket ws
run_interactive_prompt_case udp udp
run_connection_failure_notification
run_gameplay_case tcp tcp no default
run_gameplay_case websocket websocket no websocket
run_tcp_reconnection_case
run_multiple_protocol_game
run_fixed_session_reconnection_case tcp
run_fixed_session_reconnection_case websocket
run_gameplay_case udp-default default yes default
run_gameplay_case udp-explicit udp no default
run_gameplay_case tcp-contact tcp no tcp
run_gameplay_case tcp-contact-udp-game udp no tcp
if test -x "$x11_client"; then
    run_x11_local_discovery_case
    run_x11_gameplay_case tcp
    run_x11_gameplay_case websocket
fi
run_transport_mismatch udp tcp
run_transport_mismatch tcp udp

if ! runtime_logs_have_no_gl_errors; then
    fail "OpenGL diagnostics reported a runtime error"
fi

echo "SDL3 E2E smoke passed"
