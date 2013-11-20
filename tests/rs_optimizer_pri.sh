# Test for the RainerScript optimizer, folding of
# syslogfacility/priority-text to prifilt. Unfortunately, we cannot yet
# automatically detect if the optimizer does not correctly fold, but we
# can at least detect if it segfaults or otherwise creates incorrect code.
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-20
echo ===============================================================================
echo \[rs_optimizer_pri.sh\]: testing RainerScript PRI optimizer
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rs_optimizer_pri.conf
sleep 1
source $srcdir/diag.sh tcpflood -m100 # correct facility
source $srcdir/diag.sh tcpflood -m100 -P175 # incorrect facility --> must be ignored
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 99
source $srcdir/diag.sh exit
