# Test for queue data persisting at shutdown. The
# plan is to start an instance, emit some data, do a relatively
# fast shutdown and then re-start the engine to process the 
# remaining data.
# added 2009-05-27 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo \[daqueue-persist-drvr.sh\]: testing memory daqueue persisting to disk, mode $1
source $srcdir/diag.sh init

#export RSYSLOG_DEBUG="debug nologfuncflow nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"

# prepare config
echo \$MainMsgQueueType $1 > work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > work-delay.conf

# inject 10000 msgs, so that DO hit the high watermark
source $srcdir/diag.sh startup queue-persist.conf
source $srcdir/diag.sh injectmsg 0 10000
$srcdir/diag.sh shutdown-immediate
$srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh check-mainq-spool

echo "Enter phase 2, rsyslogd restart"

exit

# restart engine and have rest processed
#remove delay
echo "#" > work-delay.conf
source $srcdir/diag.sh startup queue-persist.conf
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
$srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 99999
source $srcdir/diag.sh exit
