#!/bin/bash
# Keep the oldest one-message batch inside a confirmed omprog call while later
# unequal batches retire and wrap a 128-slot FixedArray many times. The helper
# announces receipt before blocking on a FIFO; only the test can release it.
# All later message IDs must reach omfile BEFORE release, proving count-based
# RAM reclamation does not wait for the oldest batch. After release, exact
# sequence coverage and proper termination detect overwritten pointers, loss,
# duplicates, and cleanup errors. JSON trees exercise final reference cleanup.
# The 120-second omprog timeout is only a hang bound, never synchronization.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4097

generate_conf
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
    set $!tree = parse_json("{\"nested\":{\"array\":[1,2,3],\"text\":\"payload\"}}");
    if ($msg contains "msgnum:00000000:") then
        action(type="omprog" binary="'$PWD'/'$RSYSLOG_DYNNAME'.helper"
            confirmMessages="on" confirmTimeout="120000"
            template="outfmt")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
wait_content '00000000' "$RSYSLOG_DYNNAME.entered"
injectmsg 1 $((NUMMESSAGES - 1))
wait_file_lines "$RSYSLOG_OUT_LOG" $((NUMMESSAGES - 1))
check_not_present '00000000' "$RSYSLOG_OUT_LOG"
printf 'release\n' > "$RSYSLOG_DYNNAME.release"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
