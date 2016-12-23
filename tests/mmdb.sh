#!/bin/bash 
# added 2014-11-17 by singh.janmejay 
# This file is part of the rsyslog project, released under ASL 2.0 
echo =============================================================================== 
echo \[mmdb.sh\]: test for mmdb 
. $srcdir/diag.sh init 
. $srcdir/diag.sh startup mmdb.conf 
. $srcdir/diag.sh tcpflood -m 1 -j '202.106.0.20'
echo doing shutdown 
. $srcdir/diag.sh shutdown-when-empty 
echo wait on shutdown 
. $srcdir/diag.sh wait-shutdown  
. $srcdir/diag.sh content-check '"city" : "Beijing"' 
. $srcdir/diag.sh exit 
