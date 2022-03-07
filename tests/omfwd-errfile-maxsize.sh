#!/bin/bash
# part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

export MAX_ERROR_SIZE=1999

generate_conf
add_conf '
action(type="omfwd" target="1.2.3.4" port="1234" Protocol="tcp" NetworkNamespace="doesNotExist"
       action.errorfile="'$RSYSLOG2_OUT_LOG'" action.errorfile.maxsize="'$MAX_ERROR_SIZE'")
'
startup
shutdown_when_empty
wait_shutdown
check_file_exists ${RSYSLOG2_OUT_LOG}
file_size_check ${RSYSLOG2_OUT_LOG} ${MAX_ERROR_SIZE}
exit_test
