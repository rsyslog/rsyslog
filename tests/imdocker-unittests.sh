#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/imdocker/.libs/imdocker")
'

# enable debug output
export RS_REDIR=-d
startup > $RSYSLOG_OUT_LOG
wait_shutdown

grep "\\[imdocker unit test\\] all unit tests pass\\."  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected message not found.  $RSYSLOG_OUT_LOG is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
