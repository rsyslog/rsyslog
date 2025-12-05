#!/bin/bash
## @brief Validate SRV discovery for omfwd when resolving targets via DNS SRV over UDP.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=40
DNS_PORT=$(get_free_port)
DNS_RECORDS="$RSYSLOG_DYNNAME.srv.json"
UDP_PORT=$(get_free_port)
UDP_OUT="$RSYSLOG_OUT_LOG"

cat >"$DNS_RECORDS" <<EOFJSON
{
  "SRV": {
    "_syslog._udp.example.test.": [
      {"priority": 5, "weight": 0, "port": $UDP_PORT, "target": "udp1.example.test."}
    ]
  },
  "A": {
    "udp1.example.test.": "127.0.0.1"
  }
}
EOFJSON

python3 "$srcdir/dns_srv_mock.py" --port "$DNS_PORT" --records "$DNS_RECORDS" &
DNS_PID=$!
sleep 1

RESOLV_BACKUP="$RSYSLOG_DYNNAME.resolv.bak"
cp /etc/resolv.conf "$RESOLV_BACKUP"
trap 'kill $DNS_PID 2>/dev/null; kill $UDP_PID 2>/dev/null; cp "$RESOLV_BACKUP" /etc/resolv.conf' EXIT
cat > /etc/resolv.conf <<EOFRESOLV
nameserver 127.0.0.1
options port:$DNS_PORT attempts:1 timeout:1
EOFRESOLV

python3 - <<'PY' "$UDP_PORT" "$UDP_OUT" "$NUMMESSAGES" &
import socket
import sys
import time
port = int(sys.argv[1])
outfile = sys.argv[2]
expected = int(sys.argv[3])
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", port))
sock.settimeout(15)
received = 0
end_time = time.time() + 20
with open(outfile, "w", encoding="utf-8", errors="ignore") as f:
    while received < expected and time.time() < end_time:
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue
        f.write(data.decode(errors="ignore") + "\n")
        f.flush()
        received += 1
sys.exit(0)
PY
UDP_PID=$!

sleep 1

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
        action(type="omfwd" targetSrv="example.test" protocol="udp")
}
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

kill $DNS_PID 2>/dev/null
kill $UDP_PID 2>/dev/null
cp "$RESOLV_BACKUP" /etc/resolv.conf
trap - EXIT

if [ "$(wc -l < "$UDP_OUT")" -ne "$NUMMESSAGES" ]; then
    echo "Unexpected UDP message count"
    exit 1
fi

exit_test
