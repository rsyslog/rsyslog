#!/bin/bash
## @brief Validate SRV discovery for omfwd when resolving targets via DNS SRV over UDP.
. ${srcdir:=.}/diag.sh init
check_command_available python3

export NUMMESSAGES=40
DNS_RECORDS="$RSYSLOG_DYNNAME.srv.json"
UDP_PORT=$(get_free_port)
UDP_OUT="$RSYSLOG_OUT_LOG"
DNS_PORT_FILE="$RSYSLOG_DYNNAME.dns_port"

cat >"$DNS_RECORDS" <<EOFJSON
{
  "SRV": {
    "_syslog._udp.example.test.": [
      {"priority": 5, "weight": 0, "port": $UDP_PORT, "target": "127.0.0.1."}
    ]
  }
}
EOFJSON

python3 "$srcdir/dns_srv_mock.py" --port 0 --port-file "$DNS_PORT_FILE" --records "$DNS_RECORDS" &
DNS_PID=$!
wait_file_exists "$DNS_PORT_FILE"
DNS_PORT=$(cat "$DNS_PORT_FILE")

export RSYSLOG_DNS_SERVER=127.0.0.1
export RSYSLOG_DNS_PORT="$DNS_PORT"
export RES_OPTIONS="attempts:1 timeout:1"
trap 'kill $DNS_PID 2>/dev/null; kill $UDP_PID 2>/dev/null; rm -f "$DNS_PORT_FILE"' EXIT

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
        message = data.decode(errors="ignore")
        message = message.rstrip("\n")
        f.write(message + "\n")
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
rm -f "$DNS_PORT_FILE"
unset RSYSLOG_DNS_SERVER RSYSLOG_DNS_PORT
unset RES_OPTIONS
trap - EXIT

if [ "$(wc -l < "$UDP_OUT")" -ne "$NUMMESSAGES" ]; then
    echo "Unexpected UDP message count"
    exit 1
fi

exit_test
