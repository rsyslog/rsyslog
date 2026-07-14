#!/bin/bash
# An existing empty segmented queue directory has no backlog to reconstruct.
# The oracle is successful startup and clean shutdown: missing state is fatal
# only when queue files exist, while this case must create fresh state.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
mkdir -p "$SPOOL_DIR/mainq.segq"

generate_conf
add_conf '
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.saveOnShutdown="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
exit_test
