#!/bin/bash
# Verify that duplicate RainerScript lookup_table names are rejected during
# config validation. This covers the issue #5316 regression path where repeated
# generated definitions created unreachable lookup tables and one reloader
# thread per duplicate before reporting the real config problem.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl")
lookup_table(name="xlate" file="'$RSYSLOG_DYNNAME'.xlate.lkp_tbl")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

cp -f "$srcdir/testsuites/xlate.lkp_tbl" "$RSYSLOG_DYNNAME.xlate.lkp_tbl"
../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -eq 0 ]; then
    echo "FAIL: expected config validation failure for duplicate lookup_table name"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "lookup_table: duplicate name 'xlate' in current config set" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected duplicate lookup_table name error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

exit_test
