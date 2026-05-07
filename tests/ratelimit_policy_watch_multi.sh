#!/bin/bash
# Test shared watched-file scheduling across multiple ratelimit policies.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

read -r PORT_A PORT_B < <(get_free_ports 2 udp | tr '\n' ' ')
export PORT_A PORT_B
export POLICY_A="$(pwd)/${RSYSLOG_DYNNAME}.policy-a.yaml"
export POLICY_B="$(pwd)/${RSYSLOG_DYNNAME}.policy-b.yaml"
export OUT_A="$(pwd)/${RSYSLOG_DYNNAME}.out-a.log"
export OUT_B="$(pwd)/${RSYSLOG_DYNNAME}.out-b.log"
export SENDMESSAGES=20

cat > "$POLICY_A" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

cat > "$POLICY_B" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML

generate_conf
add_conf '
global(processInternalMessages="on")
ratelimit(name="watch_a" policy="'$POLICY_A'" policyWatch="on" policyWatchDebounce="200ms")
ratelimit(name="watch_b" policy="'$POLICY_B'" policyWatch="on" policyWatchDebounce="200ms")
module(load="../plugins/imudp/.libs/imudp" batchSize="1")
input(type="imudp" port="'$PORT_A'" ratelimit.name="watch_a" ruleset="a")
input(type="imudp" port="'$PORT_B'" ratelimit.name="watch_b" ruleset="b")

template(name="outfmt" type="string" string="RECEIVED RAW: %rawmsg%\n")

ruleset(name="a") {
    action(type="omfile" file="'$OUT_A'" template="outfmt")
}

ruleset(name="b") {
    action(type="omfile" file="'$OUT_B'" template="outfmt")
}
'

count_msgs() {
    if [ -f "$1" ]; then
        grep -c "msgnum:" "$1" || true
    else
        echo 0
    fi
}

startup

tcpflood -Tudp -p"$PORT_A" -m "$SENDMESSAGES" -M "msgnum:"
tcpflood -Tudp -p"$PORT_B" -m "$SENDMESSAGES" -M "msgnum:"
wait_file_lines "$OUT_A" 20 100
wait_file_lines "$OUT_B" 20 100

: > "$OUT_A"
: > "$OUT_B"

cat > "$POLICY_A" <<'YAML'
interval: 10
burst: 0
severity: 0
YAML

cat > "$POLICY_B" <<'YAML'
interval: 10
burst: 0
severity: 0
YAML
./msleep 1500

tcpflood -Tudp -p"$PORT_A" -m "$SENDMESSAGES" -M "msgnum:"
tcpflood -Tudp -p"$PORT_B" -m "$SENDMESSAGES" -M "msgnum:"
./msleep 1000
wait_queueempty

count_a=$(count_msgs "$OUT_A")
count_b=$(count_msgs "$OUT_B")
if [ "$count_a" -ne 0 ] || [ "$count_b" -ne 0 ]; then
    echo "FAIL: restrictive reload expected both watched policies to block, got A=$count_a B=$count_b"
    error_exit 1
fi

cat > "$POLICY_A" <<'YAML'
interval: 1
burst: 1000
severity: 0
YAML
./msleep 1500

tcpflood -Tudp -p"$PORT_A" -m "$SENDMESSAGES" -M "msgnum:"
tcpflood -Tudp -p"$PORT_B" -m "$SENDMESSAGES" -M "msgnum:"
wait_file_lines "$OUT_A" 20 100
./msleep 1000
wait_queueempty

count_a=$(count_msgs "$OUT_A")
count_b=$(count_msgs "$OUT_B")
if [ "$count_a" -ne 20 ]; then
    echo "FAIL: expected policy A to reload back to permissive mode, got $count_a messages"
    error_exit 1
fi
if [ "$count_b" -ne 0 ]; then
    echo "FAIL: expected policy B to remain restrictive, got $count_b messages"
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
