#!/bin/bash
# added 2015-03-02 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_nonarray_looping.sh\]: test to assert attempt to iterate upon a non-array json-object fails gracefully
. $srcdir/diag.sh init json_nonarray_looping.sh
. $srcdir/diag.sh startup json_array_looping.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_nonarray_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh assert-content-missing 'quux'
. $srcdir/diag.sh exit
