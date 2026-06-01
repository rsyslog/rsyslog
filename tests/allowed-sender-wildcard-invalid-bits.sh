#!/bin/bash
# Regression test for malformed $AllowedSender wildcard masks.
#
# A hostname wildcard is stored in the same NetAddr union as a numeric socket
# address. When parsing the following invalid /bits value fails, cleanup must
# release only the active union member. The oracle is a clean config-check
# failure with the normal parser diagnostic, not a double-free abort.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$AllowedSender TCP,*.example.invalid/not-a-number
'

rsyslogd_config_check >"${RSYSLOG_DYNNAME}.config-check.log" 2>&1
status=$?

if [ "$status" -eq 0 ]; then
    echo "Expected configuration failure for invalid AllowedSender mask"
    error_exit 1
fi
if [ "$status" -gt 128 ]; then
    echo "rsyslogd config check exited like it received a signal: $status"
    cat "${RSYSLOG_DYNNAME}.config-check.log"
    error_exit 1
fi

custom_content_check "Error -3005 parsing address in allowed senderlist" "${RSYSLOG_DYNNAME}.config-check.log"
if grep -Ei "double free|corruption|aborted|sigabrt" "${RSYSLOG_DYNNAME}.config-check.log"; then
    echo "Unexpected allocator or abort diagnostic in config-check output"
    error_exit 1
fi
exit_test
