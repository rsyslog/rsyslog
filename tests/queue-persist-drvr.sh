#!/bin/bash
# Test for queue data persisting at shutdown. The
# plan is to start an instance, emit some data, do a relatively
# fast shutdown and then re-start the engine to process the 
# remaining data.
# added 2009-05-27 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo testing memory queue persisting to disk, mode $1
. $srcdir/diag.sh init

# prepare config
echo \$MainMsgQueueType $1 > work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > work-delay.conf

# inject 5000 msgs, so that we do not hit the high watermark
. $srcdir/diag.sh startup queue-persist.conf
. $srcdir/diag.sh injectmsg 0 5000
. $srcdir/diag.sh shutdown-immediate
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh check-mainq-spool

# restart engine and have rest processed
#remove delay
echo "#" > work-delay.conf
. $srcdir/diag.sh startup queue-persist.conf
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
./msleep 1000
$srcdir/diag.sh wait-shutdown
# note: we need to permit duplicate messages, as due to the forced
# shutdown some messages may be flagged as "unprocessed" while they
# actually were processed. This is inline with rsyslog's philosophy
# to better duplicate than loose messages. Duplicate messages are
# permitted by the -d seq-check option.
. $srcdir/diag.sh seq-check 0 4999 -d
. $srcdir/diag.sh exit
