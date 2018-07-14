#!/bin/bash
# Test for Linkedlist queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under GPLv3
. $srcdir/diag.sh init
. $srcdir/diag.sh startup linkedlistqueue.conf
. $srcdir/diag.sh injectmsg  0 40000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check 0 39999
. $srcdir/diag.sh exit
