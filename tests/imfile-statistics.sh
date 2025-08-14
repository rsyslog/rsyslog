#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
generate_conf
# NOTE: do NOT set a working directory!
add_conf '
module(load="../plugins/imfile/.libs/imfile")

# Enable impstats to see the stats
module(load="../plugins/impstats/.libs/impstats"
	log.file="./'$RSYSLOG_DYNNAME'.stats.log"
	log.syslog="off"
	format="json"
	resetCounters="off"
	interval="1"
)

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" tag="file:")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsgs")
'
# make sure file exists when rsyslog starts up
touch $RSYSLOG_DYNNAME.input
startup
./msleep 2000
./inputfilegen -m $NUMMESSAGES >> $RSYSLOG_DYNNAME.input
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
content_check "imfile: no working or state file directory set" $RSYSLOG_DYNNAME.othermsgs

# content_check 'global: origin=imfile' $RSYSLOG_OUT_LOG.stats.log
echo /Users/ahallur/PersonalWorkspace/rsyslog_og/tests/$RSYSLOG_DYNNAME.stats.log

EXPECTED_BYTES=$((17 * NUMMESSAGES)) # Test data is 17 bytes per line
EXPECTED_LINES=$((NUMMESSAGES))
content_check --regex '^.*{ \"name\": \".*'$RSYSLOG_DYNNAME'.input\", \"origin\": \"imfile\".*\"bytes.processed\": '$EXPECTED_BYTES'.*$' $RSYSLOG_DYNNAME.stats.log
content_check --regex '^.*{ \"name\": \".*'$RSYSLOG_DYNNAME'.input\", \"origin\": \"imfile\".*\"lines.processed\": '$EXPECTED_LINES'.*$' $RSYSLOG_DYNNAME.stats.log
exit_test preserve
