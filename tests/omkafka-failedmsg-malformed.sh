#!/usr/bin/env bash
# Regression coverage for GitHub issue #4452's restart-crash class. omkafka
# must ignore malformed persisted failed-message records instead of
# dereferencing a missing tab separator while loading replay state.
. ${srcdir:=.}/diag.sh init
require_plugin omkafka

failed_msg_file="${RSYSLOG_DYNNAME}.failedmsg"
printf 'malformed persisted failed message without separators\n' > "$failed_msg_file"

generate_conf
add_conf '
module(load="../plugins/omkafka/.libs/omkafka")

template(name="outfmt" type="string" string="%msg%\n")

action(type="omkafka"
       topic="unreachable"
       broker="127.0.0.1:9"
       template="outfmt"
       resubmitOnFailure="on"
       keepFailedMessages="on"
       failedMsgFile="'$failed_msg_file'"
       action.resumeRetryCount="0")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
rc=$?
if [ "$rc" -eq 139 ]; then
    echo "FAIL: omkafka crashed while loading malformed failed-message state"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "omkafka: loadFailedMsgs droping invalid msg found" "${RSYSLOG_DYNNAME}.log"

exit_test
