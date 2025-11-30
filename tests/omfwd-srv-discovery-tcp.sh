#!/bin/bash
## @brief Validate SRV discovery for omfwd in TCP mode, ensuring SRV weight is honored.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=80
DNS_PORT=$(get_free_port)
DNS_RECORDS="$RSYSLOG_DYNNAME.srv.json"

start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1
start_minitcpsrvr "$RSYSLOG2_OUT_LOG" 2

cat >"$DNS_RECORDS" <<EOFJSON
{
  "SRV": {
    "_syslog._tcp.example.test.": [
      {"priority": 10, "weight": 70, "port": $MINITCPSRVR_PORT1, "target": "srv1.example.test."},
      {"priority": 10, "weight": 30, "port": $MINITCPSRVR_PORT2, "target": "srv2.example.test."}
    ]
  },
  "A": {
    "srv1.example.test.": "127.0.0.1",
    "srv2.example.test.": "127.0.0.1"
  }
}
EOFJSON

python3 "$srcdir/dns_srv_mock.py" --port "$DNS_PORT" --records "$DNS_RECORDS" &
DNS_PID=$!
sleep 1

RESOLV_BACKUP="$RSYSLOG_DYNNAME.resolv.bak"
cp /etc/resolv.conf "$RESOLV_BACKUP"
trap 'kill $DNS_PID 2>/dev/null; cp "$RESOLV_BACKUP" /etc/resolv.conf' EXIT
cat > /etc/resolv.conf <<EOFRESOLV
nameserver 127.0.0.1
options port:$DNS_PORT attempts:1 timeout:1
EOFRESOLV

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
        action(type="omfwd" targetSrv="example.test" protocol="tcp" pool.resumeInterval="5")
}
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

kill $DNS_PID 2>/dev/null
cp "$RESOLV_BACKUP" /etc/resolv.conf
trap - EXIT

count1=$(wc -l < "$RSYSLOG_OUT_LOG")
count2=$(wc -l < "$RSYSLOG2_OUT_LOG")
if [ "$((count1 + count2))" -ne "$NUMMESSAGES" ]; then
    echo "Unexpected total message count: $count1 + $count2"
    exit 1
fi
if [ "$count1" -eq 0 ] || [ "$count2" -eq 0 ]; then
    echo "Expected messages on both TCP targets, got $count1/$count2"
    exit 1
fi

exit_test
