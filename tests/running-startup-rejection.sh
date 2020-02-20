#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

# this test attempts to start a second running instance with the same pidfile

. ${srcdir:=.}/diag.sh init

generate_conf
# NOTE: do NOT set a working directory!
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" tag="file:")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsgs")
'

# make sure file exists when rsyslog starts up
touch $RSYSLOG_DYNNAME.input

# the rejected session will log here
REJECT_LOG="$RSYSLOG_DYNNAME.rejected_session.out"
touch "$REJECT_LOG"

startup
RS_REDIR="> $REJECT_LOG 2>&1" startup_forced_second_instance

# the failed attempt to start the instance should show up in the log
wait_content "is locked by rsyslogd with valid lock file" $REJECT_LOG

shutdown_immediate
wait_shutdown
exit_test
