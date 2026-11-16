#!/bin/bash
# Verify that omfwd resolves the same hostname independently per action
# network namespace when rsyslog is built with libnsgai support.
echo ===============================================================================
echo \[omfwd-nsgai-netns-dns.sh\]: test omfwd namespace-aware DNS resolution via nsgai
echo This test must be run with CAP_SYS_ADMIN capabilities [network namespace creation/change required]
if [ "$EUID" -ne 0 ]; then
	exit 77 # Not root, skip this test
fi

. ${srcdir:=.}/diag.sh init
require_netns_capable
check_command_available ip
check_command_available nsenter
check_command_available python3
check_command_available timeout

COREDNS_BIN="${COREDNS:-}"
if [ -z "$COREDNS_BIN" ]; then
	if command -v coredns >/dev/null 2>&1; then
		COREDNS_BIN="$(command -v coredns)"
	fi
fi
if [ -z "$COREDNS_BIN" ] || [ ! -x "$COREDNS_BIN" ]; then
	echo "coredns not found; set COREDNS or install coredns to run this test"
	exit 77
fi

NSGAI_ADIG_BIN="${NSGAI_ADIG:-}"
if [ -z "$NSGAI_ADIG_BIN" ]; then
	if command -v nsgai_adig >/dev/null 2>&1; then
		NSGAI_ADIG_BIN="$(command -v nsgai_adig)"
	fi
fi

HOST="shared.omfwd-nsgai.test"
ZONE="omfwd-nsgai.test"
NAMESERVER_IP="127.0.0.2"
generate_conf
PORT="$TCPFLOOD_PORT"
NS_PREFIX=$(basename "${RSYSLOG_DYNNAME}")
NS1="${NS_PREFIX}.omfwd-nsgai.1"
NS2="${NS_PREFIX}.omfwd-nsgai.2"
ADDR1="127.10.1.1"
ADDR2="127.10.2.1"
WORKDIR="${RSYSLOG_DYNNAME}.omfwd-nsgai"
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
	rm -rf "/etc/netns/$NS1" "/etc/netns/$NS2"
}
trap cleanup EXIT

write_coredns_config() {
	local nsdir="$1"
	local addr="$2"

	mkdir -p "$nsdir"
	cat > "${nsdir}/db.zone" <<EOF
\$ORIGIN ${ZONE}.
\$TTL 30
@      IN SOA  ns.${ZONE}. admin.${ZONE}. (1 60 60 600 30)
@      IN NS   ns.${ZONE}.
ns     IN A    ${NAMESERVER_IP}
shared IN A    ${addr}
EOF

	cat > "${nsdir}/Corefile" <<EOF
. {
    bind ${NAMESERVER_IP}
    file ${nsdir}/db.zone ${ZONE}
    errors
}
EOF
}

setup_namespace() {
	local ns="$1"
	local addr="$2"
	local nsdir="$3"

	ip netns add "$ns"
	ip netns exec "$ns" ip link set dev lo up
	ip netns exec "$ns" ip addr add "${addr}/32" dev lo
	mkdir -p "/etc/netns/$ns"
	cat > "/etc/netns/$ns/resolv.conf" <<EOF
nameserver ${NAMESERVER_IP}
options timeout:1 attempts:2 ndots:0
EOF
	write_coredns_config "$nsdir" "$addr"
	nsenter --net="/var/run/netns/${ns}" -- \
		"$COREDNS_BIN" -conf "${nsdir}/Corefile" \
		>"${nsdir}/coredns.log" 2>&1 &
	PIDS+=("$!")
}

wait_for_dns() {
	local ns="$1"
	local addr="$2"
	local log="$3"
	local pid="$4"
	local out=""

	for _ in $(seq 1 50); do
		if [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null; then
			echo "CoreDNS in $ns exited before becoming ready"
			echo "CoreDNS log:"
			tail -80 "$log" 2>/dev/null || true
			error_exit 1
		fi
		if [ -n "$NSGAI_ADIG_BIN" ] && [ -x "$NSGAI_ADIG_BIN" ]; then
			out="$(timeout 1 env LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" \
				"$NSGAI_ADIG_BIN" -4 -n "$ns" -p dns-only "$HOST" 2>&1 || true)"
		else
			out="$(ip netns exec "$ns" timeout 1 python3 - "$HOST" "$NAMESERVER_IP" <<'PY'
import random
import socket
import struct
import sys

host = sys.argv[1].rstrip(".")
server = sys.argv[2]
txid = random.randrange(0, 65536)
flags = 0x0100
header = struct.pack("!HHHHHH", txid, flags, 1, 0, 0, 0)
qname = b"".join(len(label).to_bytes(1, "big") + label.encode("ascii")
                 for label in host.split(".")) + b"\x00"
question = qname + struct.pack("!HH", 1, 1)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1.0)
sock.sendto(header + question, (server, 53))
data, _ = sock.recvfrom(4096)
sock.close()

resp_txid, _, qdcount, ancount, _, _ = struct.unpack("!HHHHHH", data[:12])
if resp_txid != txid:
    raise SystemExit("mismatched DNS transaction id")

offset = 12
for _ in range(qdcount):
    while data[offset] != 0:
        offset += 1 + data[offset]
    offset += 1 + 4

answers = []
for _ in range(ancount):
    if data[offset] & 0xC0 == 0xC0:
        offset += 2
    else:
        while data[offset] != 0:
            offset += 1 + data[offset]
        offset += 1
    rtype, rclass, _, rdlen = struct.unpack("!HHIH", data[offset:offset + 10])
    offset += 10
    rdata = data[offset:offset + rdlen]
    offset += rdlen
    if rtype == 1 and rclass == 1 and rdlen == 4:
        answers.append(socket.inet_ntoa(rdata))

for answer in answers:
    print(answer)
PY
2>&1 || true)"
		fi
		if printf '%s\n' "$out" | grep -qxF "$addr"; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 100
	done

	echo "CoreDNS in $ns did not resolve $HOST to $addr"
	echo "last readiness probe output:"
	printf '%s\n' "$out"
	echo "CoreDNS log:"
	tail -80 "$log" 2>/dev/null || true
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
setup_namespace "$NS1" "$ADDR1" "${WORKDIR}/ns1"
PID_NS1="${PIDS[$((${#PIDS[@]} - 1))]}"
setup_namespace "$NS2" "$ADDR2" "${WORKDIR}/ns2"
PID_NS2="${PIDS[$((${#PIDS[@]} - 1))]}"
wait_for_dns "$NS1" "$ADDR1" "${WORKDIR}/ns1/coredns.log" "$PID_NS1"
wait_for_dns "$NS2" "$ADDR2" "${WORKDIR}/ns2/coredns.log" "$PID_NS2"

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

if $msg contains "omfwd-nsgai-ns1:" then {
	action(type="omfwd" template="outfmt"
	       target="'$HOST'" port="'$PORT'" protocol="tcp"
	       networknamespace="'$NS1'")
}
if $msg contains "omfwd-nsgai-ns2:" then {
	action(type="omfwd" template="outfmt"
	       target="'$HOST'" port="'$PORT'" protocol="tcp"
	       networknamespace="'$NS2'")
}
'

startup
for i in $(seq 1 100); do
	injectmsg_literal "<167>1 2003-03-01T01:00:00.000Z host testprog - - - omfwd-nsgai-ns1: msgnum:${i}"
	injectmsg_literal "<167>1 2003-03-01T01:00:00.000Z host testprog - - - omfwd-nsgai-ns2: msgnum:${i}"
done
shutdown_when_empty
wait_shutdown

for pid in "${PIDS[@]}"; do
	kill "$pid" >/dev/null 2>&1
	wait "$pid" >/dev/null 2>&1
done
PIDS=()

verify_output "$OUT1" "omfwd-nsgai-ns1:" "omfwd-nsgai-ns2:" 100
verify_output "$OUT2" "omfwd-nsgai-ns2:" "omfwd-nsgai-ns1:" 100
exit_test
