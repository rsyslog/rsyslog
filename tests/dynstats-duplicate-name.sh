#!/bin/bash
# Verify that duplicate RainerScript dyn_stats bucket names produce a config
# warning while keeping the config valid for backward compatibility. The oracle
# is successful -N1 config validation plus the diagnostic; no runtime message
# processing output is involved.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
dyn_stats(name="dup_bucket" resettable="off" maxCardinality="3")
dyn_stats(name="dup_bucket" resettable="off" maxCardinality="9")

if $msg contains "never" then {
    set $.inc = dyn_inc("dup_bucket", "unused");
}
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: expected config validation to continue after duplicate dyn_stats bucket warning"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "dynstats: duplicate bucket name 'dup_bucket' in current config set; impstats output may be ambiguous" \
    "${RSYSLOG_DYNNAME}.log"

exit_test
