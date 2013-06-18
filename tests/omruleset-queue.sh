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
source $srcdir/diag.sh init
source $srcdir/diag.sh startup omruleset-queue.conf
source $srcdir/diag.sh injectmsg  0 20000
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check 0 19999
source $srcdir/diag.sh exit
