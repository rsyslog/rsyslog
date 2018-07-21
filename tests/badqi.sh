#!/bin/bash
# Test for a startup with a bad qi file. This tests simply tests
# if the rsyslog engine survives (we had segfaults in this situation
# in the past).
# added 2009-10-21 by RGerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[badqi.sh\]: test startup with invalid .qi file
. $srcdir/diag.sh init
startup badqi.conf
# we just inject a handful of messages so that we have something to wait for...
. $srcdir/diag.sh tcpflood -m20
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown  # wait for process to terminate
seq_check 0 19
exit_test
