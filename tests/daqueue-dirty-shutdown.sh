#!/bin/bash
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
	   queue.timeoutworkerthreadshutdown="500" queue.size="40000"
	   )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
#:msg, contains, "msgnum:" ?dynfile;outfmt
:msg, contains, "msgnum:" :omtesting:sleep 10 0
'
. $srcdir/diag.sh startup
$srcdir/diag.sh injectmsg  0 60000
echo spool files immediately before shutdown:
ls test-spool
. $srcdir/diag.sh shutdown-immediate # shut down without the ability to fully persist state
./msleep 150	# simulate an os timeout (let it run a *very short* bit, else it's done ;))
echo spool files immediately after shutdown \(but before kill\):
ls test-spool


. $srcdir/diag.sh kill-immediate   # do not give it sufficient time to shutdown
echo spool files after kill:
ls test-spool

if [ ! -f test-spool/mainq.qi ]; then
    echo "FAIL: .qi file does not exist!"
    . $srcdir/diag.sh error-exit 1
fi

# We leave this inside the test -- maybe we can later improve our success predicate
# Note that the problem is that we *do have* some data loss because we unconditionally
# kill rsyslog, so we really don't know the number we need to check after restart.
# This means the test is probably as good as possible
#. $srcdir/diag.sh startup
#. $srcdir/diag.sh shutdown-when-empty 
#. $srcdir/diag.sh wait-shutdown
#. $srcdir/diag.sh seq-check 0 19999

. $srcdir/diag.sh exit
