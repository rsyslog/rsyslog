#!/bin/bash
# Test for multiple ports in imtcp
# This test checks if multiple tcp listener ports are correctly
# handled by imtcp
#
# NOTE: this test must (and can) be enhanced when we merge in the
#       upgraded tcpflood program
#
# added 2009-05-22 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[imtcp-multiport.sh\]: testing imtcp multiple listeners
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imtcp-multiport.conf
. $srcdir/diag.sh tcpflood -p13514 -m10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit
#
#
# ### now complete new cycle with other port ###
#
#
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imtcp-multiport.conf
. $srcdir/diag.sh tcpflood -p13515 -m10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit
#
#
# ### now complete new cycle with other port ###
#
#
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imtcp-multiport.conf
. $srcdir/diag.sh tcpflood -p13516 -m10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit
