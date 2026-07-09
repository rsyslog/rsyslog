#!/bin/bash
# Verify the experimental segmentedDisk pure-disk queue can write, drain, and
# delete segmented queue files. The oracle is final sequence correctness; line
# count is used only to synchronize shutdown once all messages reached omfile.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="8k"
	queue.dequeueBatchSize="64"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syslog.log")
'

startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
if find "${RSYSLOG_DYNNAME}.spool" -maxdepth 1 \( -name 'mainq.segq' -o -name 'mainq.*.seg' \) | grep -q .; then
	echo "FAIL: segmentedDisk left queue files behind after clean drain"
	find "${RSYSLOG_DYNNAME}.spool" -maxdepth 1 \( -name 'mainq.segq' -o -name 'mainq.*.seg' \) -print
	error_exit 1
fi
check_not_present "open error" "$RSYSLOG_DYNNAME.syslog.log"
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
