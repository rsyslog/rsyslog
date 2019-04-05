#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$DebugLevel 2
$DebugFile '$RSYSLOG_OUT_LOG'
module(load="../contrib/imdocker/.libs/imdocker")
'

startup
shutdown_when_empty

grep "\\[imdocker unit test\\] all unit tests pass\\."  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected message not found.  $RSYSLOG_OUT_LOG is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
