#!/bin/bash
# add 2017-10-30 by PascalWithopf, released under ASL 2.0
#tests for Segmentation Fault
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
#set $!r = $!var1!var3!var2;
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

. $srcdir/diag.sh exit
