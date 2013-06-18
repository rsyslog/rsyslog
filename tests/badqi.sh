# Test for a startup with a bad qi file. This tests simply tests
# if the rsyslog engine survives (we had segfaults in this situation
# in the past).
# added 2009-10-21 by RGerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[badqi.sh\]: test startup with invalid .qi file
source $srcdir/diag.sh init
source $srcdir/diag.sh startup badqi.conf
# we just inject a handful of messages so that we have something to wait for...
source $srcdir/diag.sh tcpflood -m20
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown  # wait for process to terminate
source $srcdir/diag.sh seq-check 0 19
source $srcdir/diag.sh exit
