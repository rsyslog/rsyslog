#!/bin/bash
# Regression coverage for empty rendered templates reaching omprog. The
# stimulus is a real action invocation whose configured string template renders
# to the empty C string. The oracle is rsyslogd's proper-termination marker:
# before the guard in omprog, ASan reports a one-byte under-read in the
# trailing-LF check and rsyslogd aborts before writing this marker.
. ${srcdir:=.}/diag.sh init

set_proper_termination_file
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")
template(name="empty" type="string" string="")

if $rawmsg contains "trigger-empty-template" then {
	action(type="omprog" binary="/bin/cat" template="empty")
}
'

startup
injectmsg_literal 'trigger-empty-template'
shutdown_when_empty
wait_shutdown
check_proper_termination

exit_test
