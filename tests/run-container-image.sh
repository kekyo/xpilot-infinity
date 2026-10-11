#!/bin/sh

set -eu

container_engine=${CONTAINER_ENGINE:-podman}
test_root=
tcp_container=
configured_container=
recording_container=
udp_container=

fail()
{
    echo "run-container-image.sh: $*" >&2
    dump_container_logs
    exit 1
}

skip()
{
    echo "SKIP: $*" >&2
    exit 77
}

assert_equal()
{
    expected=$1
    actual=$2
    message=$3

    test "$expected" = "$actual" \
        || fail "$message (expected '$expected', got '$actual')"
}

container_is_running()
{
    test "$("$container_engine" inspect \
        --format '{{.State.Running}}' "$1" 2>/dev/null || true)" = true
}

wait_for_container_log()
{
    container_name=$1
    expected_text=$2
    wait_description=$3
    wait_deadline=$(($(date +%s) + 30))

    while ! "$container_engine" logs "$container_name" 2>&1 \
        | grep -F -- "$expected_text" >/dev/null 2>&1
    do
        container_is_running "$container_name" \
            || fail "$container_name stopped before $wait_description"
        test "$(date +%s)" -lt "$wait_deadline" \
            || fail "timed out waiting for $wait_description"
        sleep 0.2
    done
}

reserve_tcp_port()
{
    node -e '
const net = require("node:net");
const server = net.createServer();
server.unref();
server.on("error", () => process.exit(1));
server.listen(0, "127.0.0.1", () => {
  process.stdout.write(String(server.address().port));
  server.close();
});'
}

reserve_udp_range()
{
    node -e '
const dgram = require("node:dgram");
const width = 4;

const closeAll = async (sockets) => {
  await Promise.all(sockets.map((socket) => new Promise((resolve) => {
    try {
      socket.close(resolve);
    } catch {
      resolve();
    }
  })));
};

const bind = (socket, port) => new Promise((resolve, reject) => {
  socket.once("error", reject);
  socket.bind(port, "127.0.0.1", () => {
    socket.removeAllListeners("error");
    resolve();
  });
});

(async () => {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    const start = 20000 + Math.floor(Math.random() * (40000 - width));
    const sockets = [];
    try {
      for (let offset = 0; offset < width; offset += 1) {
        const socket = dgram.createSocket("udp4");
        sockets.push(socket);
        await bind(socket, start + offset);
      }
      await closeAll(sockets);
      process.stdout.write(String(start));
      return;
    } catch {
      await closeAll(sockets);
    }
  }
  process.exit(1);
})().catch(() => process.exit(1));'
}

native_container_architecture()
{
    case $(uname -m) in
        x86_64|amd64) printf '%s\n' amd64 ;;
        aarch64|arm64) printf '%s\n' arm64 ;;
        *) fail "unsupported native container architecture: $(uname -m)" ;;
    esac
}

run_contact_probe()
{
    target=$1
    expected_text=$2
    output_path=$3

    printf 'S\nJ\n' \
        | "$contact_probe" --interactive "$target" \
            >"$output_path" 2>&1 \
        || fail "the client protocol probe could not join $target"
    grep -F -- "$expected_text" "$output_path" >/dev/null 2>&1 \
        || fail "the client protocol probe did not report: $expected_text"
}

stop_container()
{
    container_name=$1
    "$container_engine" stop --time 10 "$container_name" >/dev/null \
        || fail "could not stop $container_name"
    container_is_running "$container_name" \
        && fail "$container_name remained running after SIGTERM"
    "$container_engine" logs "$container_name" 2>&1 \
        | grep -F 'Terminating on signal 15' >/dev/null 2>&1 \
        || fail "$container_name did not perform its SIGTERM cleanup"
}

dump_container_logs()
{
    for container_name in \
        "$tcp_container" "$configured_container" \
        "$recording_container" "$udp_container"
    do
        if test -n "$container_name" \
            && "$container_engine" container exists "$container_name" \
                >/dev/null 2>&1
        then
            echo "===== $container_name =====" >&2
            "$container_engine" logs "$container_name" 2>&1 \
                | sed -n '1,240p' >&2 || true
        fi
    done
    if test -n "$test_root"; then
        for output_path in "$test_root"/*.log; do
            if test -f "$output_path"; then
                echo "===== $output_path =====" >&2
                sed -n '1,240p' "$output_path" >&2
            fi
        done
    fi
}

test -n "${XPILOT_CONTAINER_BUILDER:-}" \
    || fail "XPILOT_CONTAINER_BUILDER is not set"
test -n "${XPILOT_CONTAINER_SOURCE_DIR:-}" \
    || fail "XPILOT_CONTAINER_SOURCE_DIR is not set"
test -n "${XPILOT_CONTACT_TARGET_PROBE:-}" \
    || fail "XPILOT_CONTACT_TARGET_PROBE is not set"

container_builder=$XPILOT_CONTAINER_BUILDER
source_dir=$XPILOT_CONTAINER_SOURCE_DIR
contact_probe=$XPILOT_CONTACT_TARGET_PROBE

test -x "$container_builder" \
    || fail "container build script is unavailable: $container_builder"
test -f "$source_dir/Dockerfile" \
    || fail "Dockerfile is unavailable: $source_dir/Dockerfile"
test -x "$contact_probe" \
    || fail "client protocol probe is unavailable: $contact_probe"
command -v "$container_engine" >/dev/null 2>&1 \
    || skip "$container_engine is unavailable"
command -v node >/dev/null 2>&1 || skip "Node.js is unavailable"
"$container_engine" info >/dev/null 2>&1 \
    || skip "$container_engine cannot access its container storage"

test_root=$(mktemp -d "${TMPDIR:-/tmp}/xpilot-container-test.XXXXXX")
test_suffix=$$
image_ref="localhost/xpilot-infinity-server-test:$test_suffix"
tcp_container="xpilot-container-tcp-$test_suffix"
configured_container="xpilot-container-configured-$test_suffix"
recording_container="xpilot-container-recording-$test_suffix"
udp_container="xpilot-container-udp-$test_suffix"
volume_name="xpilot-container-data-$test_suffix"
secret_name="xpilot-container-password-$test_suffix"

cleanup()
{
    for container_name in \
        "$tcp_container" "$configured_container" \
        "$recording_container" "$udp_container"
    do
        "$container_engine" rm --force "$container_name" \
            >/dev/null 2>&1 || true
    done
    "$container_engine" volume rm --force "$volume_name" \
        >/dev/null 2>&1 || true
    "$container_engine" secret rm "$secret_name" \
        >/dev/null 2>&1 || true
    "$container_engine" image rm --force "$image_ref" \
        >/dev/null 2>&1 || true
    case "$test_root" in
        "${TMPDIR:-/tmp}"/xpilot-container-test.*)
            rm -rf -- "$test_root"
            ;;
    esac
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

test_commit=0123456701234567012345670123456701234567
CONTAINER_ENGINE=$container_engine "$container_builder" \
    --version 9.8.7 \
    --revision "$test_commit" \
    --tag "$image_ref" \
    --jobs 2

assert_equal 10001:10001 \
    "$("$container_engine" image inspect \
        --format '{{.Config.User}}' "$image_ref")" \
    "the image did not select the non-root runtime user"
assert_equal "$(native_container_architecture)" \
    "$("$container_engine" image inspect \
        --format '{{.Architecture}}' "$image_ref")" \
    "the image architecture did not match the native build platform"
"$container_engine" run --rm "$image_ref" -version \
    >"$test_root/version.log" 2>&1 || true
grep -Fx "XPilot Infinity [9.8.7-$test_commit]" \
    "$test_root/version.log" \
    >/dev/null 2>&1 \
    || fail "the requested product version was not embedded in the image"

tcp_port=$(reserve_tcp_port) \
    || fail "could not reserve a TCP contact port"
"$container_engine" run --detach \
    --name "$tcp_container" \
    --read-only \
    --cap-drop=all \
    --security-opt=no-new-privileges \
    --publish "127.0.0.1:$tcp_port:15345/tcp" \
    "$image_ref" >/dev/null \
    || fail "could not start the default TCP container"
wait_for_container_log "$tcp_container" \
    'Contact transport: tcp on port 15345.' \
    "the default TCP listener"
assert_equal 10001 \
    "$("$container_engine" exec "$tcp_container" /usr/bin/id -u)" \
    "the running server was not using the non-root UID"
run_contact_probe "tcp://127.0.0.1:$tcp_port" \
    '[Contact/Lobby: TCP, Gameplay: TCP]' \
    "$test_root/tcp-probe.log"
stop_container "$tcp_container"

cat >"$test_root/defaults.txt" <<'EOF'
framesPerSecond: 37
EOF
cat >"$test_root/password.txt" <<'EOF'
password: \override: container-test-password
EOF
chmod 0600 "$test_root/password.txt"
"$container_engine" secret create "$secret_name" \
    "$test_root/password.txt" >/dev/null \
    || fail "could not create the operator password secret"
"$container_engine" volume create "$volume_name" >/dev/null \
    || fail "could not create the persistent data volume"

configured_port=$(reserve_tcp_port) \
    || fail "could not reserve the configured TCP contact port"
"$container_engine" run --detach \
    --name "$configured_container" \
    --read-only \
    --cap-drop=all \
    --security-opt=no-new-privileges \
    --publish "127.0.0.1:$configured_port:15345/tcp" \
    --volume "$test_root/defaults.txt:/etc/xpilot/defaults.txt:ro" \
    --volume "$source_dir/lib/maps/ndh.xp2:/maps/container-test.xp2:ro" \
    --volume "$volume_name:/var/lib/xpilot-infinity-server:rw" \
    --secret "source=$secret_name,target=xpilot-password,uid=10001,gid=10001,mode=0400" \
    "$image_ref" \
    -noQuit +reportMeta -tcp \
    -map /maps/container-test.xp2 \
    -defaultsFileName /etc/xpilot/defaults.txt \
    -passwordFileName /run/secrets/xpilot-password \
    >/dev/null \
    || fail "could not start the configured TCP container"
wait_for_container_log "$configured_container" \
    'Server runs at 37 frames per second' \
    "the mounted defaults file to take effect"
"$container_engine" exec "$configured_container" /bin/sh -ec \
    'test -r /run/secrets/xpilot-password
     test "$(stat -c %u:%g:%a /run/secrets/xpilot-password)" = 10001:10001:400
     grep -Fq "password:" /run/secrets/xpilot-password' \
    || fail "the Podman secret was not readable with the requested ownership"
run_contact_probe "tcp://127.0.0.1:$configured_port" \
    '[Contact/Lobby: TCP, Gameplay: TCP]' \
    "$test_root/configured-probe.log"
stop_container "$configured_container"

"$container_engine" run --detach \
    --name "$recording_container" \
    --read-only \
    --cap-drop=all \
    --security-opt=no-new-privileges \
    --volume "$volume_name:/var/lib/xpilot-infinity-server:rw" \
    "$image_ref" \
    -noQuit +reportMeta -udp -map ndh.xp2 \
    -recordFileName container-test.xpr -recordMode 1 \
    >/dev/null \
    || fail "could not start the persistent recording container"
wait_for_container_log "$recording_container" \
    'Server runs at 60 frames per second' \
    "the persistent recording server"
stop_container "$recording_container"
"$container_engine" run --rm \
    --user 10001:10001 \
    --volume "$volume_name:/data:ro" \
    --entrypoint /bin/sh \
    "$image_ref" -ec 'test -s /data/container-test.xpr' \
    || fail "the recording was not persisted in the data volume"

udp_contact_port=$(reserve_udp_range) \
    || fail "could not reserve a host port for UDP contact"
udp_range_start=$(reserve_udp_range) \
    || fail "could not reserve a UDP gameplay port range"
udp_range_end=$((udp_range_start + 3))
"$container_engine" run --detach \
    --name "$udp_container" \
    --read-only \
    --cap-drop=all \
    --security-opt=no-new-privileges \
    --publish "127.0.0.1:$udp_contact_port:15345/udp" \
    --publish "127.0.0.1:$udp_range_start-$udp_range_end:$udp_range_start-$udp_range_end/udp" \
    "$image_ref" \
    -noQuit +reportMeta -udp -map ndh.xp2 \
    -clientPortStart "$udp_range_start" \
    -clientPortEnd "$udp_range_end" \
    >/dev/null \
    || fail "could not start the UDP compatibility container"
wait_for_container_log "$udp_container" \
    'Contact transport: udp on port 15345.' \
    "the UDP contact listener"
run_contact_probe "udp://127.0.0.1:$udp_contact_port" \
    '*** Login allowed.' "$test_root/udp-probe.log"
stop_container "$udp_container"

echo "Container image build and runtime checks passed"
