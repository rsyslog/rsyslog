#!/bin/bash
# Verify that worker-thread counts of zero are rejected in both object and
# legacy queue configuration paths.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
       queue.type="linkedList" queue.workerthreads="0")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for object queue.workerthreads=0"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "parameter 'queue.workerthreads' cannot be less than one" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected object queue.workerthreads validation error"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

generate_conf 2
add_conf '
$MainMsgQueueWorkerThreads 0
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
' 2

../tools/rsyslogd -N1 -f"${TESTCONF_NM}2.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.legacy.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for legacy mainmsgqueueworkerthreads=0"
    cat "${RSYSLOG_DYNNAME}.legacy.log"
    error_exit 1
fi

grep -F "must be greater than zero" "${RSYSLOG_DYNNAME}.legacy.log" >/dev/null || {
    echo "FAIL: expected legacy mainmsgqueueworkerthreads validation error"
    cat "${RSYSLOG_DYNNAME}.legacy.log"
    error_exit 1
}

exit_test
