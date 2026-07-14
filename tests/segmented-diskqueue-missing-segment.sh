#!/bin/bash
# Remove a middle sealed segment from valid segmentedDisk state. Lazy dequeue
# must diagnose one missing segment, increment its counter, skip the unknowable
# lost range, and continue through the final later record without startup scan.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available python3
require_plugin impstats
export NUMMESSAGES=1200
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats.log"
RSYSLOGD_LOG="$PWD/${RSYSLOG_DYNNAME}.rsyslogd.log"
export RS_REDIR=">>$RSYSLOGD_LOG 2>&1"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1")
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="8k" queue.dequeueBatchSize="32" queue.saveOnShutdown="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 10 0'
startup
wait_rsyslog_instance_pid
injectmsg
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" >"${RSYSLOG_DYNNAME}.before.json"
missing_id=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); ids=[s["id"] for s in d["segments"] if s["sealed"]]; print(ids[len(ids)//2])' \
	"${RSYSLOG_DYNNAME}.before.json")
python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" --remove-segment "$missing_id" \
	>"${RSYSLOG_DYNNAME}.removed.json"

: >"$STATS_FILE"
: >"$RSYSLOGD_LOG"
write_conf '# no delay during lazy recovery'
startup
wait_content 'startup.payloadBytesRead=0' "$STATS_FILE"
shutdown_when_empty
wait_shutdown
content_check "expected segment $missing_id is missing during lazy dequeue" "$RSYSLOGD_LOG"
wait_content 'corruption.segments=1' "$STATS_FILE"
content_check '00001199' "$RSYSLOG_OUT_LOG"
rm -rf "$SPOOL_DIR"
exit_test
