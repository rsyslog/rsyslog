#!/bin/bash
# Test for duplicate named rate limits
# This should FAIL with a configuration error
. ${srcdir:=.}/diag.sh init
if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi


generate_conf
add_conf '
ratelimit(name="test_limit" interval="2" burst="5")
ratelimit(name="test_limit" interval="1" burst="10")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

# Check config only, redirect stderr
export CONF_FILE="${TESTCONF_NM}.conf"
$RSYSLOGD -N1 -M"$RSYSLOG_MODDIR" -f $CONF_FILE > /dev/null 2> stderr.log

if grep -q "ratelimit: duplicate name 'test_limit' in current config set" stderr.log; then
    echo "SUCCESS: detected duplicate name error."
    exit_test
else
    echo "FAIL: duplicate name error not found in:"
    cat stderr.log
    error_exit 1
fi
