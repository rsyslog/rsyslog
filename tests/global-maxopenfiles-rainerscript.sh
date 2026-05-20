#!/bin/bash
# Validate the modern global(maxOpenFiles=...) setting.  The test starts
# rsyslogd so it exercises both the RainerScript global-parameter backend and
# the activation-time setrlimit path.  The oracle is a clean startup/shutdown
# using the current shell soft limit, which should be accepted without needing
# extra privileges.
. ${srcdir:=.}/diag.sh init

max_open_files="$(ulimit -n)"
if [ "${max_open_files}" = "unlimited" ]; then
	max_open_files=4096
fi

generate_conf
add_conf '
global(maxOpenFiles="'${max_open_files}'")
action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out")
'

startup
shutdown_when_empty
wait_shutdown

exit_test
