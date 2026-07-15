#!/bin/bash
# Verify segmentedDisk persists the complete event representation used by the
# legacy MsgSerialize()/MsgDeserialize() path, plus receive port, post-PRI raw
# message state, parse status, message JSON ($!), and local variables ($.).
# Each event is rendered before enqueue and again after action-queue recovery;
# exact file equality is the oracle, so type, value, ordering, or field loss is
# detected without depending on timing. A deliberately missing omfile parent
# directory keeps the action suspended until the first daemon is killed.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
REFERENCE_FILE="$PWD/${RSYSLOG_DYNNAME}.reference.log"
RECOVERED_FILE="$PWD/${RSYSLOG_DYNNAME}.recovered.log"
BLOCKED_FILE="$PWD/${RSYSLOG_DYNNAME}.missing/output.log"

if [ "${SEGDISK_EVENT_DA:-0}" = 1 ]; then
	SEGDISK_DA_QUEUE_TYPE=${SEGDISK_DA_QUEUE_TYPE:-LinkedList}
	QUEUE_CONFIG='queue.type="'"$SEGDISK_DA_QUEUE_TYPE"'"
	queue.filename="eventq"
	queue.diskQueueType="segmentedDisk"
	queue.size="20"
	queue.highWatermark="5"
	queue.lowWatermark="2"'
else
	QUEUE_CONFIG='queue.type="segmentedDisk"
	queue.filename="eventq"'
fi

write_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")

template(name="eventSnapshot" type="list") {
	constant(value="{")
	property(format="jsonf" name="msg" field.number="2" field.delimiter="58" outname="sequence") constant(value=",")
	property(format="jsonf" name="protocol-version" outname="protocol_version") constant(value=",")
	property(format="jsonf" name="syslogseverity" outname="severity") constant(value=",")
	property(format="jsonf" name="syslogfacility" outname="facility") constant(value=",")
	property(format="jsonf" name="timegenerated" dateformat="unixtimestamp" outname="generated") constant(value=",")
	property(format="jsonf" name="timegenerated" dateformat="rfc3339" outname="received") constant(value=",")
	property(format="jsonf" name="timereported" dateformat="rfc3339" outname="timestamp") constant(value=",")
	property(format="jsonf" name="syslogtag" outname="tag") constant(value=",")
	property(format="jsonf" name="rawmsg" outname="raw_message") constant(value=",")
	property(format="jsonf" name="rawmsg-after-pri" outname="raw_after_pri") constant(value=",")
	property(format="jsonf" name="msg" outname="message") constant(value=",")
	property(format="jsonf" name="hostname" outname="hostname") constant(value=",")
	property(format="jsonf" name="inputname" outname="input_name") constant(value=",")
	property(format="jsonf" name="fromhost" outname="from_host") constant(value=",")
	property(format="jsonf" name="fromhost-ip" outname="from_host_ip") constant(value=",")
	property(format="jsonf" name="fromhost-port" outname="from_host_port") constant(value=",")
	property(format="jsonf" name="structured-data" outname="structured_data") constant(value=",")
	property(format="jsonf" name="app-name" outname="app_name") constant(value=",")
	property(format="jsonf" name="procid" outname="procid") constant(value=",")
	property(format="jsonf" name="msgid" outname="msgid") constant(value=",")
	property(format="jsonf" name="parsesuccess" outname="parse_success") constant(value=",")
	property(format="jsonf" name="$!" outname="message_variables") constant(value=",")
	property(format="jsonf" name="$." outname="local_variables")
	constant(value="}\n")
}

set $.parse_result = parse_json(
	"{\"text\":\"value\",\"integer\":9223372036854775807,\"real\":1.25,\"boolean\":true,"
	& "\"null\":null,\"array\":[\"one\",2,false]}", "\$!event");
set $.parse_result = parse_json(
	"{\"text\":\"local-value\",\"integer\":7,\"boolean\":false}", "\$.local");
unset $.parse_result;

if $msg contains "msgnum:" then {
	action(type="omfile" file="'"$REFERENCE_FILE"'" template="eventSnapshot")
	action(type="omfile" file="'"$1"'" template="eventSnapshot"
		createDirs="off"
		action.resumeRetryCount="-1"
'"$QUEUE_CONFIG"'
		queue.maxFileSize="4k"
		queue.dequeueBatchSize="8"
		queue.saveOnShutdown="on")
}
'
}

write_conf "$BLOCKED_FILE"
startup
injectmsg 0 "$NUMMESSAGES"
wait_file_lines "$REFERENCE_FILE" "$NUMMESSAGES"
shutdown_immediate
if [ "${SEGDISK_EVENT_DA:-0}" != 1 ]; then
	. "$srcdir/diag.sh" kill-immediate
fi
wait_shutdown
if [ "$(wc -c < "$SPOOL_DIR/eventq.segq/state")" -ne 512 ]; then
	echo "FAIL: segmented action queue was not persisted"
	error_exit 1
fi

write_conf "$RECOVERED_FILE"
startup
wait_file_lines "$RECOVERED_FILE" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
if [ "${SEGDISK_EVENT_DA:-0}" = 1 ]; then
	# The DA shutdown path may append the final in-memory records after records
	# already spilled to disk. Sorting by the fixed-width leading sequence key
	# keeps this test focused on lossless full-event persistence, not on shutdown
	# transfer ordering, which is covered separately by the DA ordering tests.
	sort "$REFERENCE_FILE" > "${REFERENCE_FILE}.sorted"
	sort "$RECOVERED_FILE" > "${RECOVERED_FILE}.sorted"
	cmp_exact_file "${REFERENCE_FILE}.sorted" "${RECOVERED_FILE}.sorted"
else
	cmp_exact_file "$REFERENCE_FILE" "$RECOVERED_FILE"
fi
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
