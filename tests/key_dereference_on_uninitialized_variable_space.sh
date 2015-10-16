#!/bin/bash
# added 2015-05-27 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[key_dereference_on_uninitialized_variable_space.sh\]: test to dereference key from a not-yet-created cee or local json-object
. $srcdir/diag.sh init key_dereference_on_uninitialized_variable_space.sh
. $srcdir/diag.sh startup key_dereference_on_uninitialized_variable_space.conf
. $srcdir/diag.sh tcpflood -m 10
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-check 'cee:'
. $srcdir/diag.sh exit
