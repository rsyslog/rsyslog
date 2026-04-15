#!/bin/bash
# Verify that an empty queue.spooldirectory is rejected during config validation.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
       queue.type="disk" queue.filename="qf1" queue.spooldirectory="")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for empty queue.spooldirectory"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "queue.spooldirectory must not be empty" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected empty queue.spooldirectory error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

exit_test
