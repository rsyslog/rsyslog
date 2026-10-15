#!/bin/bash
# Keep the oldest one-message batch inside a confirmed omprog call while later
# unequal batches retire and wrap a 128-slot FixedArray many times. The helper
# announces receipt before blocking on a FIFO; only the test can release it.
# All later message IDs must reach omfile BEFORE release, proving count-based
# RAM reclamation does not wait for the oldest batch. After release, exact
# sequence coverage and proper termination detect overwritten pointers, loss,
# duplicates, and cleanup errors. Startup begins with an empty queue, then the
# messageful queue drains to empty before a bounded shutdown; that lifecycle
# oracle catches an idle callback reported as successful instead of idle. JSON
# trees exercise final reference cleanup. The 120-second omprog timeout is only
# a hang bound, never synchronization.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4097

generate_conf
add_conf '$AbortOnUncleanConfig on'
mkfifo "$RSYSLOG_DYNNAME.release"
cat > "$RSYSLOG_DYNNAME.helper" <<HELPER
#!/bin/bash
printf 'OK\n'
while IFS= read -r message; do
    printf '%s\n' "\$message" > "$PWD/$RSYSLOG_DYNNAME.entered"
    IFS= read -r release < "$PWD/$RSYSLOG_DYNNAME.release"
    printf 'OK\n'
done
HELPER
chmod +x "$RSYSLOG_DYNNAME.helper"
add_conf '
module(load="../plugins/omprog/.libs/omprog")
main_queue(queue.type="FixedArray" queue.size="128"
    queue.dequeueBatchSize="31" queue.workerThreads="3"
    queue.workerThreadMinimumMessages="1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $.ret = parse_json("{\"nested\":{\"array\":[1,2,3],\"text\":\"payload\"}}", "\$!tree");
    if ($!tree!nested!text != "payload") then stop
    if ($msg contains "msgnum:00000000:") then
        action(type="omprog" binary="'$PWD'/'$RSYSLOG_DYNNAME'.helper"
            confirmMessages="on" confirmTimeout="120000"
            template="outfmt")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
# The initial empty queue must not execute the ruleset before a real enqueue.
if [ -e "$RSYSLOG_OUT_LOG" ]; then
    echo "unexpected output while the queue is empty"
    error_exit 1
fi
injectmsg 0 1
wait_content '00000000' "$RSYSLOG_DYNNAME.entered"
injectmsg 1 $((NUMMESSAGES - 1))
wait_file_lines "$RSYSLOG_OUT_LOG" $((NUMMESSAGES - 1))
check_not_present '00000000' "$RSYSLOG_OUT_LOG"
printf 'release\n' > "$RSYSLOG_DYNNAME.release"
shutdown_when_empty
wait_shutdown "" 120
seq_check
exit_test
