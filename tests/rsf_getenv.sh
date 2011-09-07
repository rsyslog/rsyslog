# Test for the getenv() rainerscript function
# this is a quick test, but it gurantees that the code path is
# at least progressed (but we do not check for unset envvars!)
# added 2009-11-03 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[rsf_getenv.sh\]: testing RainerScript getenv\(\) function
export MSGNUM="msgnum:"
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rsf_getenv.conf
source $srcdir/diag.sh tcpflood -m10000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 9999
unset MSGNUM
source $srcdir/diag.sh exit
