#!/bin/bash
# Test for duplicate named rate limits.
#
# The oracle is config-validation failure with the duplicate-name diagnostic on
# stderr. Use a test-instance-specific stderr capture file because this
# negative config test runs in parallel with other validation tests.
. ${srcdir:=.}/diag.sh init
if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi

STDERR_LOG="${RSYSLOG_DYNNAME}.stderr.log"

generate_conf
add_conf '
ratelimit(name="test_limit" interval="2" burst="5")
ratelimit(name="test_limit" interval="1" burst="10")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

# Check config only, redirect stderr
export CONF_FILE="${TESTCONF_NM}.conf"
$RSYSLOGD -N1 -M"$RSYSLOG_MODDIR" -f "$CONF_FILE" > /dev/null 2> "$STDERR_LOG"

if grep -q "ratelimit: duplicate name 'test_limit' in current config set" "$STDERR_LOG"; then
    echo "SUCCESS: detected duplicate name error."
    exit_test
else
    echo "FAIL: duplicate name error not found in:"
    cat "$STDERR_LOG"
    error_exit 1
fi
