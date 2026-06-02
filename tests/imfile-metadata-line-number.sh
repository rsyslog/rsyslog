#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Regression coverage for issue #728: imfile metadata includes a persisted,
# 1-based line_number value. The oracle uses exact omfile output across two
# daemon starts to prove the value continues from the state file.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input"
      addMetadata="on" persistStateAfterSubmission="on")

template(name="outfmt" type="list") {
	property(name="msg" field.number="2" field.delimiter="58")
	constant(value=" line:")
	property(name="$!metadata!line_number")
	constant(value="\n")
}

if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

./inputfilegen -m2 > "$RSYSLOG_DYNNAME.input"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty
wait_shutdown

./inputfilegen -m2 -i2 >> "$RSYSLOG_DYNNAME.input"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 4
shutdown_when_empty
wait_shutdown

printf '00000000 line:1\n00000001 line:2\n00000002 line:3\n00000003 line:4\n' > "$RSYSLOG_DYNNAME.expected"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"

exit_test
