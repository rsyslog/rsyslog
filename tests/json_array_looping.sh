# added 2014-11-11 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_array_looping.sh\]: basic test for looping over json array
source $srcdir/diag.sh init json_array_looping.sh
source $srcdir/diag.sh startup json_array_looping.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_array_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check 'quux: abc0'
source $srcdir/diag.sh content-check 'quux: def1'
source $srcdir/diag.sh content-check 'quux: ghi2'
source $srcdir/diag.sh content-check 'quux: { "bar": [ { "baz": "important_msg" }, { "baz": "other_msg" } ] }'
source $srcdir/diag.sh content-check 'grault: { "baz": "important_msg" }'
source $srcdir/diag.sh content-check 'grault: { "baz": "other_msg" }'
source $srcdir/diag.sh content-check 'garply: important_msg, other_msg'
source $srcdir/diag.sh exit
