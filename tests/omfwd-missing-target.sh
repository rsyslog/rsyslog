#!/bin/bash
# Regression test for issue #5057: omfwd actions without target or targetSrv
# must fail during config validation instead of reaching runtime connection
# setup. The oracle is rsyslogd -N1 exiting with failure and emitting the
# actionable missing-destination diagnostic.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

action(type="omfwd"
       protocol="tcp"
       port="1514"
       template="outfmt"
       TCP_Framing="octet-counted")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -eq 0 ]; then
    echo "FAIL: expected config validation failure for omfwd without target or targetSrv"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "omfwd: either target or targetSrv must be specified" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected omfwd missing target error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

exit_test
