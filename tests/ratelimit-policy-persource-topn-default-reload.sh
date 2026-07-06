#!/bin/bash
## HUP reloads of canonical ratelimit policy YAML must treat an omitted
## perSource.topN as the documented default, not as "keep the previous value".
## The oracle uses impstats top-drop counters: after reloading from topN=1 to
## omitted topN, a second per-source top counter must be created.

. ${srcdir:=.}/diag.sh init
require_plugin impstats

export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.tcp_port"
export STATSFILE="${RSYSLOG_DYNNAME}.stats"
POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_FILE
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export INPUT_FILE

write_policy() {
    local topn_line="$1"
    cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceKey"
$topn_line
  default:
    max: 1
    window: 30s
EOF
}

send_host_messages() {
    local host="$1"
    local count="$2"
    : > "$INPUT_FILE"
    for i in $(seq 1 "$count"); do
        echo "<13>Jan 1 00:00:00 $host app: msgnum:$host:$i" >> "$INPUT_FILE"
    done
    tcpflood -p"$PORT_RCVR" -I "$INPUT_FILE"
}

wait_stats_contains() {
    local pattern="$1"
    local retries=0
    while [ "$retries" -lt 20 ]; do
        if [ -f "$STATSFILE" ] && grep -q "$pattern" "$STATSFILE"; then
            return 0
        fi
        retries=$((retries + 1))
        ./msleep 250
    done
    return 1
}

write_policy "  topN: 1"

generate_conf
add_conf '
template(name="PerSourceKey" type="string" string="%hostname%")
ratelimit(name="topn_reload" policy="'$POLICY_FILE'")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="topn_reload")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

send_host_messages alpha 4
send_host_messages beta 3

if ! wait_stats_contains "per_source_drop_1_alpha"; then
    echo "FAIL: expected first topN counter before reload"
    cat "$STATSFILE"
    error_exit 1
fi

if grep -q "per_source_drop_2_beta" "$STATSFILE"; then
    echo "FAIL: topN=1 exposed a second topN counter before reload"
    error_exit 1
fi

write_policy ""
issue_HUP
./msleep 1000

send_host_messages alpha 2
send_host_messages beta 2
wait_queueempty

if ! wait_stats_contains "per_source_drop_2_beta"; then
    echo "FAIL: omitted topN on reload did not restore the default topN depth"
    echo "impstats content:"
    cat "$STATSFILE"
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
exit_test
