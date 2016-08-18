#!/bin/bash
# This test simulates the case where the OS force-terminates rsyslog
# before it completely finishes persisting the queue to disk. Obviously,
# there is some data loss involved, but rsyslog should try to limit it.
# Most importantly, a .qi file needs to be written at "save" places, so that
# at least the queue is kind of readable.
# To simulate the error condition, we create a DA queue with a large memory
# part and fill it via injectmsg (do NOT use tcpflood, as this would add
# complexity of TCP window etc to the reception of messages - injecmsg is
# synchronous, so we do not have anything in flight after it terminates).
# We have a blocking action which prevents actual processing of any of the
# injected messages. We then inject a large number of messages, but only
# few above the number the memory part of the disk can hold. So the disk queue
# begins to get used. Once injection is done, we terminate rsyslog in the
# regular way, which will cause the memory part of the queue to be written
# out. After a relatively short period, we kill -9 rsyslogd, so that it
# does not have any chance to fully persists its state (this actually is
# what happens when force-terminated by the OS).
# Then, we check that at a minimum the .qi file exists.
# Copyright (C) 2016 by Rainer Gerhards
# Released under ASL 2.0
echo ===============================================================================
# uncomment for debugging support:
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omtesting/.libs/omtesting")

# set spool locations and switch queue to disk-only mode
$WorkDirectory test-spool
main_queue(queue.filename="mainq" queue.saveonshutdown="on"
           queue.timeoutshutdown="1" queue.maxfilesize="1m"
	   queue.timeoutworkerthreadshutdown="500" queue.size="200000"
	   )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
#:msg, contains, "msgnum:" ?dynfile;outfmt
:msg, contains, "msgnum:" :omtesting:sleep 10 0
'
. $srcdir/diag.sh startup
$srcdir/diag.sh injectmsg  0 210000
echo spool files immediately before shutdown:
ls test-spool
. $srcdir/diag.sh shutdown-immediate # shut down without the ability to fully persist state
./msleep 750	# simulate an os timeout (let it run a *very short* bit, else it's done ;))
echo spool files immediately after shutdown \(but before kill\):
ls test-spool


. $srcdir/diag.sh kill-immediate   # do not give it sufficient time to shutdown
echo spool files after kill:
ls test-spool

if [ ! -f test-spool/mainq.qi ]; then
    echo "FAIL: .qi file does not exist!"
    . $srcdir/diag.sh error-exit 1
fi

exit


# We leave this inside the test -- maybe we can later improve our success predicate
# Note that the problem is that we *do have* some data loss because we unconditionally
# kill rsyslog, so we really don't know the number we need to check after restart.
# This means the test is probably as good as possible
./msleep 1000
echo RSYSLOG RESTART
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omtesting/.libs/omtesting")

# set spool locations and switch queue to disk-only mode
$WorkDirectory test-spool
main_queue(queue.filename="mainq" queue.saveonshutdown="on"
           queue.timeoutshutdown="1" queue.maxfilesize="1m"
	   queue.timeoutworkerthreadshutdown="500" queue.size="200000"
	   )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
*.* ./all.log
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty 
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 19999

. $srcdir/diag.sh exit
