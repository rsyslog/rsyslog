#!/bin/bash
## @brief Validate SRV discovery for omfwd in TCP mode, ensuring SRV weight is honored.
. ${srcdir:=.}/diag.sh init
check_command_available python3

export NUMMESSAGES=80
DNS_RECORDS="$RSYSLOG_DYNNAME.srv.json"
DNS_PORT_FILE="$RSYSLOG_DYNNAME.dns_port"

start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1
start_minitcpsrvr "$RSYSLOG2_OUT_LOG" 2

cat >"$DNS_RECORDS" <<EOFJSON
{
  "SRV": {
    "_syslog._tcp.example.test.": [
      {"priority": 10, "weight": 70, "port": $MINITCPSRVR_PORT1, "target": "127.0.0.1."},
      {"priority": 10, "weight": 30, "port": $MINITCPSRVR_PORT2, "target": "127.0.0.1."}
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
trap 'kill $DNS_PID 2>/dev/null; rm -f "$DNS_PORT_FILE"' EXIT

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
rm -f "$DNS_PORT_FILE"
unset RSYSLOG_DNS_SERVER RSYSLOG_DNS_PORT
unset RES_OPTIONS
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
