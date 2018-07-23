#!/bin/bash
# added 2018-02-07 by Harshvardhan Shrivastava
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \rscript_hash64.sh\]: test for hash64 and hash64mod script-function
. $srcdir/diag.sh init
startup_vg rscript_hash64.conf
. $srcdir/diag.sh tcpflood -m 20
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-pattern-check "^\(-2574714428477944902 -  14\|-50452361579464591 -  25\)$"
exit_test
