#!/bin/bash
# added 2014-11-11 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_array_subscripting.sh\]: basic test for json array subscripting
. $srcdir/diag.sh init
. $srcdir/diag.sh startup json_array_subscripting.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_array_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh content-check 'msg: def1 | ghi2 | important_msg | { "baz": "other_msg" } | other_msg'
. $srcdir/diag.sh exit
