#!/bin/bash
# Test for fixedArray queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh startup arrayqueue.conf

# 40000 messages should be enough
. $srcdir/diag.sh injectmsg  0 40000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check 0 39999
. $srcdir/diag.sh exit
