# Copyright (C) 2011 by Rainer Gerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[rcvr_fail_restore.sh\]: test failed receiver restore case
source $srcdir/diag.sh init
#
# STEP1: start both instances and send 1000 messages.
# Note: receiver is instance 2, sender instance 1.
#
# start up the instances. Note that the envrionment settings can be changed to
# set instance-specific debugging parameters!
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log2"
source $srcdir/diag.sh startup rcvr_fail_restore_rcvr.conf 2
#export RSYSLOG_DEBUGLOG="log"
#valgrind="valgrind"
source $srcdir/diag.sh startup rcvr_fail_restore_sender.conf
# re-set params so that new instances do not thrash it...
#unset RSYSLOG_DEBUG
#unset RSYSLOG_DEBUGLOG

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
source $srcdir/diag.sh injectmsg  1 1000
source $srcdir/diag.sh wait-queueempty
./msleep 1000 # let things settle down a bit

#
# Step 2: shutdown receiver, then send some more data, which then
# needs to go into the queue.
#

source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2

source $srcdir/diag.sh injectmsg  1001 10000
./msleep 3000 # make sure some retries happen (retry interval is set to 3 second)
source $srcdir/diag.sh get-mainqueuesize
ls -l test-spool

#
# Step 3: restart receiver, wait that the sender drains its queue
#
#export RSYSLOG_DEBUGLOG="log2"
source $srcdir/diag.sh startup rcvr_fail_restore_rcvr.conf 2
echo waiting for sender to drain queue [may need a short while]
source $srcdir/diag.sh wait-queueempty
ls -l test-spool
OLDFILESIZE=$(stat -c%s test-spool/mainq.00000001)
echo file size to expect is $OLDFILESIZE


#
# Step 4: send new data. Queue files are not permitted to grow now
# (but one file continous to exist).
#
source $srcdir/diag.sh injectmsg  11001 10
source $srcdir/diag.sh wait-queueempty

# at this point, the queue file shall not have grown. Note
# that we MUST NOT shut down the instance right now, because it
# would clean up the queue files! So we need to do our checks
# first (here!).
ls -l test-spool
NEWFILESIZE=$(stat -c%s test-spool/mainq.00000001)
if [ $NEWFILESIZE != $OLDFILESIZE ]
then
   echo file sizes do not match, expected $OLDFILESIZE, actual $NEWFILESIZE
   echo this means that data has been written to the queue file where it
   echo no longer should be written.
   # abort will happen below, because we must ensure proper system shutdown
   # HOWEVER, during actual testing it may be useful to do an exit here (so
   # that e.g. the debug log is pointed right at the correct spot).
   # exit 1
fi

#
# We now do an extra test (so this is two in one ;)) to see if the DA
# queue can be reactivated after its initial shutdown. In essence, we 
# redo steps 2 and 3.
#
# Step 5: stop receiver again, then send some more data, which then
# needs to go into the queue.
#
echo "*** done primary test *** now checking if DA can be restarted"
source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2

source $srcdir/diag.sh injectmsg  11011 10000
sleep 1 # we need to wait, otherwise we may be so fast that the receiver
# comes up before we have finally suspended the action
source $srcdir/diag.sh get-mainqueuesize
ls -l test-spool

#
# Step 6: restart receiver, wait that the sender drains its queue
#
source $srcdir/diag.sh startup rcvr_fail_restore_rcvr.conf 2
echo waiting for sender to drain queue [may need a short while]
source $srcdir/diag.sh wait-queueempty
ls -l test-spool

#
# Queue file size checks done. Now it is time to terminate the system
# and see if everything could be received (the usual check, done here
# for completeness, more or less as a bonus).
#
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown

# now it is time to stop the receiver as well
source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2

# now abort test if we need to (due to filesize predicate)
if [ $NEWFILESIZE != $OLDFILESIZE ]
then
   exit 1
fi
# do the final check
source $srcdir/diag.sh seq-check 1 21010
source $srcdir/diag.sh exit
