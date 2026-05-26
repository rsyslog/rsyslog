#!/usr/bin/env bash
# Verify gcry keyprogram length parsing rejects trailing non-numeric data. The
# oracle is rsyslogd -N1 failing with the keyprogram error, proving a length
# such as "16abc" is not accepted as 16 bytes.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
main_queue(queue.filename="mainq"
	queue.type="disk"
	queue.maxDiskSpace="4m"
	queue.maxfilesize="1m"
	queue.cry.provider="gcry"
	queue.cry.keyprogram="./'${RSYSLOG_DYNNAME}'.keyprogram"
	queue.saveonshutdown="on"
)

action(type="omfile" file="'${RSYSLOG_OUT_LOG}'")
'

{
    echo '#!/usr/bin/env bash'
    echo 'echo RSYSLOG-KEY-PROVIDER:0'
    echo 'echo 16abc'
    echo 'echo "1234567890123456"'
} >"${RSYSLOG_DYNNAME}.keyprogram"
chmod +x "${RSYSLOG_DYNNAME}.keyprogram"

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for malformed keyprogram length"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "error 3 obtaining key from program" "${RSYSLOG_DYNNAME}.log"

exit_test
