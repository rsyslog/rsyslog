#!/bin/bash
# Verify that the RainerScript optimizer folds constant comparisons.
# The oracle intentionally checks the internal debug log because this test is
# about a config-optimizer transformation, not a message-processing diagnostic.
# It also checks the normal output file to ensure the optimized-out branch did
# not emit anything.
# This file is part of the rsyslog project, released under ASL 2.0.
echo ===============================================================================
echo \[rscript_optimizer_const_cmp.sh\]: testing rainerscript constant comparison optimizer
. ${srcdir:=.}/diag.sh init
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

if "a" == "b" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown
touch "$RSYSLOG_OUT_LOG"
check_not_present "msgnum:" "$RSYSLOG_OUT_LOG"
content_check "optimizer: if with constant false expression and no else - remove" "$RSYSLOG_DEBUGLOG"
exit_test
