#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init

#export RSYSLOG_DEBUG="debug nologfuncflow nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"

# prepare config
echo \$MainMsgQueueType LinkedList > work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > work-delay.conf

# inject 10000 msgs, so that DO hit the high watermark
. $srcdir/diag.sh startup queue-persist.conf
. $srcdir/diag.sh injectmsg 0 10000
. $srcdir/diag.sh shutdown-immediate
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh check-mainq-spool
./mangle_qi -d -q test-spool/mainq.qi > tmp.qi
mv tmp.qi test-spool/mainq.qi

echo "Enter phase 2, rsyslogd restart"

# restart engine and have rest processed
#remove delay
echo "#" > work-delay.conf
. $srcdir/diag.sh startup queue-persist.conf
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999 -d
. $srcdir/diag.sh exit
