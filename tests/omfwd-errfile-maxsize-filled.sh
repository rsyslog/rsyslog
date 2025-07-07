#!/bin/bash
# part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
ERRFILE="$RSYSLOG_DYNNAME.err"
export MAX_ERROR_SIZE=1999
export INITIAL_FILE_SIZE=$((MAX_ERROR_SIZE - 100))
dd if=/dev/urandom of=${ERRFILE}  bs=1 count=${INITIAL_FILE_SIZE}
generate_conf
add_conf '
action(type="omfwd" target="1.2.3.4" port="1234" Protocol="tcp" NetworkNamespace="doesNotExist"
       action.errorfile="'$ERRFILE'" action.errorfile.maxsize="'$MAX_ERROR_SIZE'")
'
startup
shutdown_when_empty
wait_shutdown
check_file_exists ${ERRFILE}
file_size_check ${ERRFILE} ${MAX_ERROR_SIZE}
exit_test
