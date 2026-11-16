#!/bin/bash
# Verify that omfwd resolves the same hostname independently per action
# network namespace via /etc/netns/<ns>/hosts when rsyslog is built with
# libnsgai support.
echo ===============================================================================
echo \[omfwd-nsgai-netns-hosts.sh\]: test omfwd namespace-aware hosts lookup via nsgai
echo This test must be run with CAP_SYS_ADMIN capabilities [network namespace creation/change required]
if [ "$EUID" -ne 0 ]; then
	exit 77 # Not root, skip this test
fi

. ${srcdir:=.}/diag.sh init
require_netns_capable
check_command_available getent
check_command_available ip
check_command_available timeout

HOST="shared.omfwd-nsgai.test"
generate_conf
PORT="$TCPFLOOD_PORT"
NS_PREFIX=$(basename "${RSYSLOG_DYNNAME}")
NS1="${NS_PREFIX}.omfwd-nsgai-hosts.1"
NS2="${NS_PREFIX}.omfwd-nsgai-hosts.2"
ADDR1="127.11.1.1"
ADDR2="127.11.2.1"
WORKDIR="${RSYSLOG_DYNNAME}.omfwd-nsgai-hosts"
OUT1="${RSYSLOG_OUT_LOG}.ns1"
OUT2="${RSYSLOG_OUT_LOG}.ns2"
READY1="${RSYSLOG_DYNNAME}.minitcpsrv.ns1.ready"
READY2="${RSYSLOG_DYNNAME}.minitcpsrv.ns2.ready"
PIDS=()

cleanup() {
	for pid in "${PIDS[@]}"; do
		kill "$pid" >/dev/null 2>&1
		wait "$pid" >/dev/null 2>&1
	done
	ip netns delete "$NS1" >/dev/null 2>&1
	ip netns delete "$NS2" >/dev/null 2>&1
	rm -rf "/etc/netns/$NS1" "/etc/netns/$NS2" "$WORKDIR"
}
trap cleanup EXIT

setup_namespace() {
	local ns="$1"
	local addr="$2"

	ip netns add "$ns"
	ip netns exec "$ns" ip link set dev lo up
	ip netns exec "$ns" ip addr add "${addr}/32" dev lo
	mkdir -p "/etc/netns/$ns"
	cat > "/etc/netns/$ns/hosts" <<EOF
127.0.0.1 localhost
${addr} ${HOST}
EOF
	cat > "/etc/netns/$ns/resolv.conf" <<EOF
nameserver 127.0.0.53
options timeout:1 attempts:1 ndots:0
EOF
}

wait_for_hosts() {
	local ns="$1"
	local addr="$2"
	local out=""

	for _ in $(seq 1 50); do
		out="$(ip netns exec "$ns" timeout 1 getent ahostsv4 "$HOST" 2>&1 || true)"
		if printf '%s\n' "$out" | grep -q "^${addr}[[:space:]]"; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 100
	done

	echo "hosts lookup in $ns did not resolve $HOST to $addr"
	echo "last getent output:"
	printf '%s\n' "$out"
	error_exit 1
}

verify_output() {
	local file="$1"
	local expected_tag="$2"
	local wrong_tag="$3"
	local expected_count="$4"
	local count

	if [ ! -s "$file" ]; then
		echo "$file is empty"
		error_exit 1
	fi
	if grep -qF "$wrong_tag" "$file"; then
		echo "$file received messages for wrong namespace tag $wrong_tag"
		cat "$file"
		error_exit 1
	fi
	count=$(grep -cF "$expected_tag" "$file")
	if [ "$count" -ne "$expected_count" ]; then
		echo "$file has $count messages for $expected_tag, expected $expected_count"
		cat "$file"
		error_exit 1
	fi
}

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
setup_namespace "$NS1" "$ADDR1"
setup_namespace "$NS2" "$ADDR2"
wait_for_hosts "$NS1" "$ADDR1"
wait_for_hosts "$NS2" "$ADDR2"

rm -f "$READY1" "$READY2"
ip netns exec "$NS1" ./minitcpsrv -t"$ADDR1" -p"$PORT" -f "$OUT1" -L "$READY1" &
PIDS+=("$!")
ip netns exec "$NS2" ./minitcpsrv -t"$ADDR2" -p"$PORT" -f "$OUT2" -L "$READY2" &
PIDS+=("$!")
wait_file_exists "$READY1"
wait_file_exists "$READY2"

add_conf '
$MainMsgQueueTimeoutShutdown 10000
template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "omfwd-nsgai-hosts-ns1:" then {
	action(type="omfwd" template="outfmt"
	       target="'$HOST'" port="'$PORT'" protocol="tcp"
	       networknamespace="'$NS1'")
}
if $msg contains "omfwd-nsgai-hosts-ns2:" then {
	action(type="omfwd" template="outfmt"
	       target="'$HOST'" port="'$PORT'" protocol="tcp"
	       networknamespace="'$NS2'")
}
'

startup
for i in $(seq 1 100); do
	injectmsg_literal "<167>1 2003-03-01T01:00:00.000Z host testprog - - - omfwd-nsgai-hosts-ns1: msgnum:${i}"
	injectmsg_literal "<167>1 2003-03-01T01:00:00.000Z host testprog - - - omfwd-nsgai-hosts-ns2: msgnum:${i}"
done
shutdown_when_empty
wait_shutdown

for pid in "${PIDS[@]}"; do
	kill "$pid" >/dev/null 2>&1
	wait "$pid" >/dev/null 2>&1
done
PIDS=()

verify_output "$OUT1" "omfwd-nsgai-hosts-ns1:" "omfwd-nsgai-hosts-ns2:" 100
verify_output "$OUT2" "omfwd-nsgai-hosts-ns2:" "omfwd-nsgai-hosts-ns1:" 100
exit_test
