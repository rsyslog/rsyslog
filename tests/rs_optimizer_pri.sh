#!/bin/bash
# Test for the RainerScript optimizer, folding of
# syslogfacility/priority-text to prifilt. Unfortunately, we cannot yet
# automatically detect if the optimizer does not correctly fold, but we
# can at least detect if it segfaults or otherwise creates incorrect code.
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-20
echo ===============================================================================
echo \[rs_optimizer_pri.sh\]: testing RainerScript PRI optimizer
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rs_optimizer_pri.conf
sleep 1
. $srcdir/diag.sh tcpflood -m100 # correct facility
. $srcdir/diag.sh tcpflood -m100 -P175 # incorrect facility --> must be ignored
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99
. $srcdir/diag.sh exit
