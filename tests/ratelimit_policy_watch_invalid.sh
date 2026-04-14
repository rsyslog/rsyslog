#!/bin/bash
# Test watched ratelimit reload failure keeps the previous active policy.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

export PORT_RCVR="$(get_free_port)"
export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export SENDMESSAGES=20

cat > "$POLICY_FILE" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

generate_conf
add_conf '
global(processInternalMessages="on")
ratelimit(name="watch_invalid" policy="'$POLICY_FILE'" policyWatch="on" policyWatchDebounce="200ms")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" port="'$PORT_RCVR'" ratelimit.name="watch_invalid" ruleset="main")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")

ruleset(name="main") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup

tcpflood -Tudp -p"$PORT_RCVR" -m "$SENDMESSAGES" -M "msgnum:"
wait_file_lines "$RSYSLOG_OUT_LOG" 20 100
: > "$RSYSLOG_OUT_LOG"

cat > "$POLICY_FILE" <<'YAML'
interval: "unterminated
YAML
./msleep 1500

tcpflood -Tudp -p"$PORT_RCVR" -m "$SENDMESSAGES" -M "msgnum:"
wait_file_lines "$RSYSLOG_OUT_LOG" 20 100

if ! grep -q "failed to reload policy 'watch_invalid'" "$RSYSLOG_DYNNAME.started"; then
    echo "FAIL: expected watched ratelimit reload failure to be logged"
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
