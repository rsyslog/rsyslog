#!/bin/bash
# added 2018-10-24 by Rainer Gerhards
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(operatingStateFile="'$RSYSLOG_DYNNAME.osf'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
action(type="invalid-type")
'
startup
shutdown_when_empty
wait_shutdown

check_file_not_exists "$RSYSLOG_DYNNAME.osf.previous"
check_file_exists "$RSYSLOG_DYNNAME.osf"
content_check "invalid-type" "$RSYSLOG_DYNNAME.osf"
content_check "CLEAN CLOSE" "$RSYSLOG_DYNNAME.osf"
exit_test
