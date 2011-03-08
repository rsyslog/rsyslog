# Test for discard-rptdmsg functionality
# This test checks if discard-rptdmsg works. It is not a perfect test but
# will find at least segfaults and obviously not discard-rptdmsged messages.
# added 2009-07-30 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh startup discard-rptdmsg.conf
# 20000 messages should be enough - the disk test is slow enough ;)
sleep 4
source $srcdir/diag.sh tcpflood -m10 -i1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 2 10
source $srcdir/diag.sh exit
