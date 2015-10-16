#!/bin/bash
# Test for disk-only queue mode with v6+ config
# This tests defines three rulesets, each one with its own queue. Then, it
# sends data to them and checks the outcome. Note that we do need to
# use some custom code as the test driver framework does not (yet?)
# support multi-output-file operations.
# added 2013-11-14 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[rulesetmultiqueu.sh\]: testing multiple queues via rulesets
. $srcdir/diag.sh init
rm -f rsyslog.out1.log rsyslog.out2.log rsyslog.out3.log
. $srcdir/diag.sh startup rulesetmultiqueue-v6.conf
. $srcdir/diag.sh wait-startup
# now fill the three files (a bit sequentially, but they should
# still get their share of concurrency - to increase the chance
# we use three connections per set).
. $srcdir/diag.sh tcpflood -c3 -p13514 -m20000 -i0
. $srcdir/diag.sh tcpflood -c3 -p13515 -m20000 -i20000
. $srcdir/diag.sh tcpflood -c3 -p13516 -m20000 -i40000

# in this version of the imdiag, we do not have the capability to poll
# all queues for emptyness. So we do a sleep in the hopes that this will
# sufficiently drain the queues. This is race, but the best we currently
# can do... - rgerhards, 2009-11-05
sleep 2 
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
# now consolidate all logs into a single one so that we can use the
# regular check logic
cat rsyslog.out1.log rsyslog.out2.log rsyslog.out3.log > rsyslog.out.log
. $srcdir/diag.sh seq-check 0 59999
rm -f rsyslog.out1.log rsyslog.out2.log rsyslog.out3.log
. $srcdir/diag.sh exit
