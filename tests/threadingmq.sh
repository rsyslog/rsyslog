#!/bin/bash
# test many concurrent tcp connections
# we send 100,000 messages in the hopes that his puts at least a little bit
# of pressure on the threading subsystem. To really prove it, we would need to
# push messages for several minutes, but that takes too long during the 
# automatted tests (hint: do this manually after suspect changes). Thankfully,
# in practice many threading bugs result in an abort rather quickly and these
# should be covered by this test here.
# rgerhards, 2009-06-26
echo \[threadingmq.sh\]: main queue concurrency
. $srcdir/diag.sh init
. $srcdir/diag.sh startup threadingmq.conf
. $srcdir/diag.sh injectmsg 0 100000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
# we give an extra seconds for things to settle, especially
# important on slower test machines
./msleep 5000
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 99999
. $srcdir/diag.sh exit
