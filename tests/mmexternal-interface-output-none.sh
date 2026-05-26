#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Regression coverage for issue #260: interface.output="none" lets an
# external message modification helper consume messages without producing the
# JSON response required by the default protocol. Because this mode has no
# helper reply to synchronize on, the oracle waits for the helper side-effect
# file before shutdown and then checks both that file and downstream omfile.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")

template(name="outfmt" type="string" string="%msg%\n")

action(type="mmexternal"
	interface.output="none"
	binary="'${srcdir}'/testsuites/mmexternal-no-output.sh '$RSYSLOG_DYNNAME'.side")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:no-output-message"
wait_file_lines "$RSYSLOG_DYNNAME.side" 1
shutdown_when_empty
wait_shutdown

content_check "no-output-message"
content_check "no-output-message" "$RSYSLOG_DYNNAME.side"

exit_test
