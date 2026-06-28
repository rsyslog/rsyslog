#!/bin/bash
## Test that imudp inherits per-source limiting through ratelimitAddMsg().
## The key template is `%fromhost-ip%`, an optimized mode that must not need
## generic template evaluation; the observable oracle is that one UDP source is
## capped by the shared ratelimiter policy.
## The impstats sleep gives the one-second stats interval time to publish the
## zero-valued key-evaluation counters that prove the fast path avoided
## tplToString and parser calls. The submitted counter must match the delivered
## message count, proving per-source drops keep the same discard semantics as
## the regular ratelimiter instead of returning success to the module.

. ${srcdir:=.}/diag.sh init
require_plugin impstats
skip_ARM "ratelimit timing flaky on ARM"
export SENDMESSAGES=20
export NUMMESSAGES=5
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export PORT_RCVR_FILE="${RSYSLOG_DYNNAME}.imudp_port"
export STATSFILE="$RSYSLOG_DYNNAME.stats"

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceIP"
  default:
    max: 5
    window: 2s
EOF

generate_conf
add_conf '
template(name="PerSourceIP" type="string" string="%fromhost-ip%")
ratelimit(name="per_source" policy="'$POLICY_FILE'")

module(load="../plugins/imudp/.libs/imudp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'"
      ratelimit.name="per_source")

template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"

tcpflood -Tudp -p"$PORT_RCVR" -m "$SENDMESSAGES"

sleep 2
shutdown_when_empty
wait_shutdown

content_count=$(grep -c "msgnum:" "$RSYSLOG_OUT_LOG")
submitted_count=$(grep "origin=imudp" "$STATSFILE" | grep -o "submitted=[0-9]*" | sed 's/.*=//' | sort -n | tail -1)
tpl_evals=$(grep "ratelimit.per_source.per_source" "$STATSFILE" | grep -o "per_source_key_tpl_evals=[0-9]*" | tail -1 | sed 's/.*=//')
parse_evals=$(grep "ratelimit.per_source.per_source" "$STATSFILE" | grep -o "per_source_key_parse_evals=[0-9]*" | tail -1 | sed 's/.*=//')
echo "content_count: $content_count"
echo "submitted_count: $submitted_count"
echo "tpl_evals: $tpl_evals"
echo "parse_evals: $parse_evals"

if [ "$content_count" -eq "$SENDMESSAGES" ]; then
    echo "FAIL: per-source ratelimit did not drop UDP messages"
    error_exit 1
fi

if [ "$content_count" -eq 0 ]; then
    echo "FAIL: all UDP messages were lost or dropped"
    error_exit 1
fi

if [ "${submitted_count:-0}" -ne "$content_count" ]; then
    echo "FAIL: imudp submitted counter counted per-source-dropped messages"
    echo "stats content:"
    cat "$STATSFILE"
    error_exit 1
fi

if [ "${tpl_evals:-0}" -ne 0 ]; then
    echo "FAIL: optimized fromhost-ip per-source key used tplToString"
    error_exit 1
fi

if [ "${parse_evals:-0}" -ne 0 ]; then
    echo "FAIL: optimized fromhost-ip per-source key parsed messages"
    error_exit 1
fi

exit_test
