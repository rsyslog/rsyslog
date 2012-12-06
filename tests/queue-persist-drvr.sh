# Test for queue data persisting at shutdown. The
# plan is to start an instance, emit some data, do a relatively
# fast shutdown and then re-start the engine to process the 
# remaining data.
# added 2009-05-27 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo testing memory queue persisting to disk, mode $1
source $srcdir/diag.sh init

# prepare config
echo \$MainMsgQueueType $1 > work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > work-delay.conf

# inject 5000 msgs, so that we do not hit the high watermark
source $srcdir/diag.sh startup queue-persist.conf
source $srcdir/diag.sh injectmsg 0 5000
$srcdir/diag.sh shutdown-immediate
$srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh check-mainq-spool

# restart engine and have rest processed
#remove delay
echo "#" > work-delay.conf
source $srcdir/diag.sh startup queue-persist.conf
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
./msleep 500
$srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 4999
source $srcdir/diag.sh exit
