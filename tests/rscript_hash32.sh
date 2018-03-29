#!/bin/bash
# added 2018-02-07 by Harshvardhan Shrivastava
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \rscript_hash32.sh\]: test for hash32 and hash64mod script-function
. $srcdir/diag.sh init
. $srcdir/diag.sh startup rscript_hash32.conf
. $srcdir/diag.sh tcpflood -m 20
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-pattern-check "^\(746581550 -  50\|3889673532 -  32\)$"
. $srcdir/diag.sh exit
