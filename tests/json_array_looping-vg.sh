#!/bin/bash
# added 2016-03-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_array_looping-vg.sh\]: basic test for looping over json array with valgrind
. $srcdir/diag.sh init json_array_looping-vg.sh
. $srcdir/diag.sh startup-vg json_array_looping.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_array_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check 'quux: abc0'
. $srcdir/diag.sh content-check 'quux: def1'
. $srcdir/diag.sh content-check 'quux: ghi2'
. $srcdir/diag.sh content-check 'quux: { "bar": [ { "baz": "important_msg" }, { "baz": "other_msg" } ] }'
. $srcdir/diag.sh custom-content-check 'grault: { "baz": "important_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh custom-content-check 'grault: { "baz": "other_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh custom-content-check 'prefixed_grault: { "baz": "important_msg" }' 'rsyslog.out.prefixed.log'
. $srcdir/diag.sh custom-content-check 'prefixed_grault: { "baz": "other_msg" }' 'rsyslog.out.prefixed.log'
. $srcdir/diag.sh content-check 'garply: important_msg, other_msg'
. $srcdir/diag.sh exit
