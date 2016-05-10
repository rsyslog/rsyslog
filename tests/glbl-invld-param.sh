#!/bin/bash
# make sure we do not abort on invalid parameter (we 
# once had this problem)
# addd 2016-03-03 by RGerhards, released under ASL 2.0
echo \[glbl-invld-param\]: 
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(invalid="off")
global(debug.unloadModules="invalid")
action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
sleep 1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
# if we reach this point, we consider this a success.
. $srcdir/diag.sh exit
