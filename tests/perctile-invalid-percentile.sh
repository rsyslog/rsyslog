#!/usr/bin/env bash
# Verify percentile_stats rejects malformed percentile entries during config
# validation. The oracle is rsyslogd -N1 failing with the perctile validation
# diagnostic, proving non-numeric trailing text is not silently accepted as a
# valid percentile.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
percentile_stats(name="bad_bucket"
  percentiles=["95abc"]
  windowsize="1000"
  delimiter="|"
)
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for malformed percentile"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "perctile: percentile value '95abc' is invalid" "${RSYSLOG_DYNNAME}.log"

exit_test
