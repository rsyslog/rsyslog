#!/bin/bash
## HUP reload must atomically replace the per-source key policy while input
## workers are active. The oracle first observes one allowance for each of two
## hostname keys, then reloads to fromhost-ip and observes one shared allowance
## for two new hostnames arriving from the same TCP peer address.

. ${srcdir:=.}/diag.sh init

export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.tcp_port"
POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_FILE
INPUT_FILE="$(pwd)/${RSYSLOG_DYNNAME}.input"
export INPUT_FILE

write_policy() {
    local key_template="$1"
    cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "$key_template"
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

write_policy "PerSourceHostname"

generate_conf
add_conf '
template(name="PerSourceHostname" type="string" string="%hostname%")
template(name="PerSourceAddress" type="string" string="%fromhost-ip%")
ratelimit(name="key_reload" policy="'$POLICY_FILE'")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="key_reload")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

send_host_messages alpha 2
send_host_messages beta 2
wait_file_lines "$RSYSLOG_OUT_LOG" 2 100

write_policy "PerSourceAddress"
issue_HUP

send_host_messages gamma 2
send_host_messages delta 2
wait_queueempty

shutdown_when_empty
wait_shutdown

content_count=$(grep -c "msgnum:" "$RSYSLOG_OUT_LOG" || true)
if [ "$content_count" -ne 3 ]; then
    echo "FAIL: expected two hostname allowances and one shared address allowance, got $content_count"
    cat "$RSYSLOG_OUT_LOG"
    error_exit 1
fi
content_check "msgnum:alpha:1" "$RSYSLOG_OUT_LOG"
content_check "msgnum:beta:1" "$RSYSLOG_OUT_LOG"
content_check "msgnum:gamma:1" "$RSYSLOG_OUT_LOG"
check_not_present "msgnum:delta:" "$RSYSLOG_OUT_LOG"

exit_test
