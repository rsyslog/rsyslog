#!/bin/bash
# Test for the getenv() rainerscript function
# this is a quick test, but it gurantees that the code path is
# at least progressed (but we do not check for unset envvars!)
# added 2009-11-03 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[rsf_getenv.sh\]: testing RainerScript getenv\(\) function
export MSGNUM="msgnum:"
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rsf_getenv.conf
. $srcdir/diag.sh tcpflood -m10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 9999
unset MSGNUM
. $srcdir/diag.sh exit
