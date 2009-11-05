# Test for fixedArray queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[arrayqueue.sh\]: testing queue fixedArray queue mode
source $srcdir/diag.sh init
source $srcdir/diag.sh startup arrayqueue.conf

# 40000 messages should be enough
source $srcdir/diag.sh injectmsg  0 40000

# terminate *now* (don't wait for queue to drain!)
kill `cat rsyslog.pid`

# now wait until rsyslog.pid is gone (and the process finished)
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check 0 39999
source $srcdir/diag.sh exit
