#!/bin/bash
# check that global ruleset queue defaults can be specified. However,
# we do not tests that they actually work - that's quite hard to
# do reliably.
# add 2019-05-09 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global( default.ruleset.queue.timeoutshutdown="1000"
	default.ruleset.queue.timeoutactioncompletion="1000"
	default.ruleset.queue.timeoutenqueue="1000"
	default.ruleset.queue.timeoutworkerthreadshutdown="1000"
	)
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
cp -f $srcdir/testsuites/xlate.lkp_tbl $RSYSLOG_DYNNAME.xlate.lkp_tbl
startup
shutdown_when_empty
wait_shutdown
check_not_present 'parameter.*not known'
exit_test
