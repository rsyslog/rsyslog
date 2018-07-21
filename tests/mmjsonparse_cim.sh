#!/bin/bash
# added 2014-07-15 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmjsonparse_cim.sh\]: basic test for mmjsonparse module with "cim" cookie
. $srcdir/diag.sh init
startup mmjsonparse_cim.conf
. $srcdir/diag.sh tcpflood -m 5000 -j "@cim: "
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
seq_check  0 4999
exit_test
