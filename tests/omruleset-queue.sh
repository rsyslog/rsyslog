#!/bin/bash
# test for omruleset. What we do is have the main queue forward 
# all messages to a secondary ruleset via omruleset, which then does
# the actual file write. We check if all messages arrive at the file, 
# what implies that omruleset works. No filters or special queue modes
# are used, but the ruleset uses its own queue. So we can also inject
# more messages without running into troubles.
# added 2009-11-02 by rgerhards
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[omruleset-queue.sh\]: test for omruleset functionality with a ruleset queue

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh startup omruleset-queue.conf
. $srcdir/diag.sh injectmsg  0 20000
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check 0 19999
. $srcdir/diag.sh exit
