# added 2015-03-02 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_nonarray_looping.sh\]: test to assert attempt to iterate upon a non-array json-object fails gracefully
source $srcdir/diag.sh init json_nonarray_looping.sh
source $srcdir/diag.sh startup json_array_looping.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_nonarray_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh exit
