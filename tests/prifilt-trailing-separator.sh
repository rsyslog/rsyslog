#!/bin/bash
# Regression test for https://github.com/rsyslog/rsyslog/issues/7546, reported
# by LuciyVI. A classic PRI selector ending in ';' must complete configuration
# validation without reading past its terminating NUL. The Valgrind wrapper
# makes any invalid read fail with its distinct exit status.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
local0.info; stop
'

rsyslogd_command=(../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR")
if [ "$USE_VALGRIND" = "YES" ]; then
    rsyslogd_command=(valgrind --error-exitcode=10 --leak-check=no --malloc-fill=ff --free-fill=fe \
        --suppressions="$srcdir/known_issues.supp" "${rsyslogd_command[@]}")
fi

"${rsyslogd_command[@]}" >"${RSYSLOG_DYNNAME}.log" 2>&1
rsyslogd_status=$?
if [ "$rsyslogd_status" -ne 0 ]; then
    echo "FAIL: trailing PRI separator config validation returned $rsyslogd_status"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit "$rsyslogd_status"
fi

content_check "End of config validation run" "${RSYSLOG_DYNNAME}.log"

exit_test
