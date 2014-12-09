# added 2014-11-11 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_array_subscripting.sh\]: basic test for json array subscripting
source $srcdir/diag.sh init
source $srcdir/diag.sh startup json_array_subscripting.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_array_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check 'msg: def1 | ghi2 | important_msg | { "baz": "other_msg" } | other_msg'
source $srcdir/diag.sh exit
