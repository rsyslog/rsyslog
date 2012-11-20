# added 2010-08-11 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_addtlframedelim.sh\]: test imptcp additional frame delimiter
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imptcp_addtlframedelim.conf
source $srcdir/diag.sh tcpflood -m20000 -F0 -P129
#sleep 2 # due to large messages, we need this time for the tcp receiver to settle...
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
source $srcdir/diag.sh seq-check 0 19999
source $srcdir/diag.sh exit
