#!/bin/sh

set -eu

server=${XPILOT_SERVER_BINARY:?XPILOT_SERVER_BINARY is required}
probe=${XPILOT_CONTACT_TARGET_PROBE:?XPILOT_CONTACT_TARGET_PROBE is required}
map=${XPILOT_TEST_PKGDATADIR:?XPILOT_TEST_PKGDATADIR is required}/maps/ndh.xp2
runtime_dir=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-multiple-test.XXXXXX")
server_pid=
holder_pid=

cleanup()
{
    for process_id in "$server_pid" "$holder_pid"; do
        if test -n "$process_id"; then
            kill -TERM "$process_id" 2>/dev/null || true
            wait "$process_id" 2>/dev/null || true
        fi
    done
    rm -rf -- "$runtime_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

fail()
{
    echo "Multiple protocol test failed: $*" >&2
    for log in "$runtime_dir"/*.log; do
        test ! -f "$log" || cat "$log" >&2
    done
    exit 1
}

wait_ready()
{
    ready_deadline=$(($(date +%s) + 30))
    until grep -q 'Server runs at' "$runtime_dir/server.log"; do
        kill -0 "$server_pid" 2>/dev/null || fail "server exited before readiness"
        test "$(date +%s)" -lt "$ready_deadline" || fail "server did not become ready"
        sleep 0.05
    done
}

stop_server()
{
    kill -TERM "$server_pid"
    wait "$server_pid" 2>/dev/null || true
    server_pid=
}

reject_configuration()
{
    set +e
    timeout 10s "$server" -map "$map" -noQuit +reportMeta "$@" \
        >"$runtime_dir/invalid.log" 2>&1
    status=$?
    set -e
    test "$status" -ne 0 && test "$status" -ne 124 \
        || fail "invalid configuration was not rejected: $*"
    if grep -q 'Server runs at' "$runtime_dir/invalid.log"; then
        fail "invalid configuration reached game readiness: $*"
    fi
}

for invalid in '' 'udp' 'udp:0' 'udp:65536' 'udp:1,' \
    'udp:1,udp:2' 'tcp:1,websocket:1' 'sctp:1'; do
    reject_configuration -listen "$invalid"
done
reject_configuration -listen tcp:15346 -port 15345
reject_configuration -listen tcp:15346 -udp
reject_configuration -listen websocket:15347 -gameTransport tcp
reject_configuration -listen udp:15345,tcp:15345 -reportMeta
reject_configuration -listen tcp:15346 -recordMode 1 \
    -recordFileName "$runtime_dir/unsupported.xpr"
reject_configuration -listen websocket:15347 -recordMode 2 \
    -recordFileName "$runtime_dir/unsupported.xpr"
reject_configuration -tcp -recordMode 1 -recordFileName "$runtime_dir/unsupported.xpr"
test ! -e "$runtime_dir/unsupported.xpr" || fail "unsupported recording was created"

# Hold all three TCP/UDP port pairs together so the kernel cannot return
# the same number for two endpoints while selecting the test ports.
node -e '
const net = require("node:net");
const dgram = require("node:dgram");
const pairs = [];
const reserve = () => {
  const tcp = net.createServer();
  tcp.listen(0, "127.0.0.1", () => {
    const port = tcp.address().port;
    const udp = dgram.createSocket("udp4");
    udp.once("error", () => { udp.close(); tcp.close(reserve); });
    udp.bind(port, "127.0.0.1", () => {
      pairs.push({ tcp, udp, port });
      if (pairs.length < 3) reserve();
      else {
        process.stdout.write(pairs.map((pair) => pair.port).join(" "));
        for (const pair of pairs) { pair.udp.close(); pair.tcp.close(); }
      }
    });
  });
};
reserve();' >"$runtime_dir/ports"
read -r udp_port tcp_port ws_port <"$runtime_dir/ports" || true
test -n "$ws_port" || fail "could not select endpoint ports"

for mask in 1 2 3 4 5 6 7; do
    listeners=
    targets=
    if test "$((mask & 1))" -ne 0; then
        listeners="udp:$udp_port"
        targets="udp://127.0.0.1:$udp_port"
    fi
    if test "$((mask & 2))" -ne 0; then
        listeners="${listeners:+$listeners,}tcp:$tcp_port"
        targets="$targets tcp://127.0.0.1:$tcp_port"
    fi
    if test "$((mask & 4))" -ne 0; then
        listeners="${listeners:+$listeners,}websocket:$ws_port"
        targets="$targets ws://127.0.0.1:$ws_port"
    fi
    "$server" -map "$map" -noQuit +reportMeta -listen "$listeners" \
        >"$runtime_dir/server.log" 2>&1 &
    server_pid=$!
    wait_ready
    for target in $targets; do
        "$probe" --status "$target" >"$runtime_dir/probe.log" 2>&1 \
            || fail "contact failed for $target in $listeners"
    done
    stop_server
done

# A port conflict after opening the first listener must fail the whole startup.
node -e '
const net = require("node:net");
const server = net.createServer();
server.listen(Number(process.argv[1]), "0.0.0.0", () => console.log("ready"));
' "$ws_port" >"$runtime_dir/holder.log" 2>&1 &
holder_pid=$!
holder_deadline=$(($(date +%s) + 10))
until grep -q ready "$runtime_dir/holder.log"; do
    kill -0 "$holder_pid" 2>/dev/null || fail "port holder exited"
    test "$(date +%s)" -lt "$holder_deadline" || fail "port holder did not start"
    sleep 0.05
done
reject_configuration -listen "udp:$udp_port,tcp:$tcp_port,websocket:$ws_port"
kill -TERM "$holder_pid"
wait "$holder_pid" 2>/dev/null || true
holder_pid=
"$server" -map "$map" -noQuit +reportMeta \
    -listen "udp:$udp_port,tcp:$tcp_port,websocket:$ws_port" \
    >"$runtime_dir/server.log" 2>&1 &
server_pid=$!
wait_ready
"$probe" --status "tcp://127.0.0.1:$tcp_port" >"$runtime_dir/probe.log" 2>&1 \
    || fail "failed startup left an endpoint unavailable"
stop_server

for admission_case in duplicate udp-duplicate version capacity ip accepted recording; do
    first_port=$tcp_port
    case "$admission_case" in
        duplicate) expected='name is already in use'; limit=8; per_ip=8 ;;
        udp-duplicate) expected='name is already in use'; limit=8; per_ip=8; first_port=$udp_port ;;
        version) expected='incompatible protocol version'; limit=8; per_ip=8 ;;
        capacity) expected='game is full'; limit=1; per_ip=8 ;;
        ip) expected='game is locked'; limit=8; per_ip=1 ;;
        accepted) expected=accepted; limit=8; per_ip=8 ;;
        recording) expected=unsupported; limit=8; per_ip=8 ;;
    esac
    "$server" -map "$map" -noQuit +reportMeta \
        -listen "udp:$udp_port,tcp:$tcp_port,websocket:$ws_port" \
        -playerLimit "$limit" -maxPerIP "$per_ip" \
        -recordFileName "$runtime_dir/unsupported.xpr" \
        >"$runtime_dir/server.log" 2>&1 &
    server_pid=$!
    wait_ready
    "${XPILOT_SERVER_ADMISSION_PROBE:?XPILOT_SERVER_ADMISSION_PROBE is required}" \
        "$first_port" "$ws_port" "$admission_case" "$expected" \
        >"$runtime_dir/admission.log" 2>&1 \
        || fail "cross-protocol admission: $admission_case"
    stop_server
    test ! -e "$runtime_dir/unsupported.xpr" || fail "runtime tuning enabled unsupported recording"
done

echo "All seven protocol sets and startup validation passed"
# Observe each endpoint from its own UDP source port, without a public service.
node -e '
const fs = require("node:fs");
const dgram = require("node:dgram");
const dir = process.argv[1];
const socket = dgram.createSocket("udp4");
socket.on("message", (packet, peer) => {
  const text = packet.toString().replace(/\0/g, "");
  const kind = text.includes("\nremove") ? "removed" : "update";
  fs.writeFileSync(`${dir}/meta-${peer.port}.${kind}`, text);
});
socket.bind(0, "127.0.0.1", () => {
  fs.writeFileSync(`${dir}/meta.port`, String(socket.address().port));
});
' "$runtime_dir" >"$runtime_dir/meta-fixture.log" 2>&1 &
holder_pid=$!
meta_deadline=$(($(date +%s) + 10))
until test -s "$runtime_dir/meta.port"; do
    kill -0 "$holder_pid" 2>/dev/null || fail "meta fixture stopped"
    test "$(date +%s)" -lt "$meta_deadline" || fail "meta fixture not ready"
    sleep 0.05
done
XPILOT_META_REPORT_HOST=127.0.0.1 XPILOT_META_REPORT_HOST_TWO=127.0.0.1 \
XPILOT_META_REPORT_PORT=$(cat "$runtime_dir/meta.port") \
    "$server" -map "$map" -noQuit -reportMeta \
    -listen "udp:$udp_port,tcp:$tcp_port,websocket:$ws_port" \
    >"$runtime_dir/server.log" 2>&1 &
server_pid=$!
wait_ready
for endpoint in "udp:$udp_port" "tcp:$tcp_port" "websocket:$ws_port"; do
    protocol=${endpoint%:*}
    port=${endpoint#*:}
    meta_deadline=$(($(date +%s) + 15))
    until test -s "$runtime_dir/meta-$port.update"; do
        test "$(date +%s)" -lt "$meta_deadline" || fail "missing $protocol advertisement from UDP port $port"
        sleep 0.05
    done
    grep -Fq "+ct=$protocol+gt=$protocol" "$runtime_dir/meta-$port.update" \
        || fail "wrong advertised protocol for $endpoint"
    grep -qx "add port $port" "$runtime_dir/meta-$port.update" \
        || fail "wrong advertised port for $endpoint"
    grep -qx 'add users 0' "$runtime_dir/meta-$port.update" \
        || fail "wrong advertised users for $endpoint"
done
stop_server
for port in "$udp_port" "$tcp_port" "$ws_port"; do
    meta_deadline=$(($(date +%s) + 10))
    until test -s "$runtime_dir/meta-$port.removed"; do
        test "$(date +%s)" -lt "$meta_deadline" || fail "missing removal from UDP port $port"
        sleep 0.05
    done
done
kill -TERM "$holder_pid"
wait "$holder_pid" 2>/dev/null || true
holder_pid=

echo "All endpoint advertisements and removals passed"
