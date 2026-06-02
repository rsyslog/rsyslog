#!/usr/bin/env bash
# Regression coverage for GitHub issue #4199. ommysql must suspend cleanly when
# the database connection is unavailable while transaction support is active.
# The oracle is synchronized shutdown plus the normal ommysql diagnostic in
# rsyslog's configured output path; the test must not crash or hang.
. ${srcdir:=.}/diag.sh init
require_plugin ommysql

export NUMMESSAGES=1
mysql_port="$(get_free_port)"

generate_conf
add_conf '
module(load="../plugins/ommysql/.libs/ommysql")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
	action(type="ommysql"
		server="127.0.0.1"
		serverport="'$mysql_port'"
		db="'$RSYSLOG_DYNNAME'"
		uid="rsyslog"
		pwd="testbench"
		queue.type="Direct"
		action.resumeRetryCount="0"
		action.resumeInterval="1")
}

syslog.* {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup
injectmsg
shutdown_when_empty
wait_shutdown

content_check "ommysql:" "$RSYSLOG_OUT_LOG"
check_not_present "Segmentation fault" "$RSYSLOG_OUT_LOG"

exit_test
