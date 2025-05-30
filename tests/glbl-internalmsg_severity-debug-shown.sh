#!/bin/bash
# check that info-severity messages are actually emitted; we use
# lookup table as a simple sample to get such a message.
# addd 2019-05-07 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(internalmsg.severity="debug")
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl" reloadOnHUP="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
startup
shutdown_when_empty
wait_shutdown
content_check "rsyslogd fully started up and initialized - begin actual processing"
exit_test
