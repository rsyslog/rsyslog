#!/bin/bash
# Verify both the explicit legacy selector and an omitted selector leave queued
# ruleset calls on the existing submitMsg2 path. The omitted case deliberately
# uses queue.size="0", which reservedBatch rejects as unbounded, so successful
# configuration and exact delivery also prove the default selector is legacy.
# Exact delivery after synchronized shutdown is the runtime oracle.
if [[ "$1" != "--case" ]]; then
    for selector_case in explicit omitted; do
        RSTB_SELECTOR_CASE="$selector_case" "$0" --case || exit $?
    done
    exit 0
fi

. ${srcdir:=.}/diag.sh init
generate_conf
if [[ "$RSTB_SELECTOR_CASE" == "explicit" ]]; then
    add_conf 'global(executionEngine="legacy")'
    target_queue_size=1
else
    target_queue_size=0
fi
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="'$target_queue_size'" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then call target
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 10 "$RSYSLOG_OUT_LOG"
exit_test
