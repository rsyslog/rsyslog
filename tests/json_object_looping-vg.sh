#!/bin/bash
# added 2016-03-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_object_looping-vg.sh\]: basic test for looping over json object / associative-array with valgrind
. $srcdir/diag.sh init json_object_looping-vg.sh
. $srcdir/diag.sh startup-vg json_object_looping.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_object_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check 'quux: { "key": "str1", "value": "abc0" }'
. $srcdir/diag.sh content-check 'quux: { "key": "str2", "value": "def1", "random_key": "str2" }'
. $srcdir/diag.sh content-check 'quux: { "key": "str3", "value": "ghi2" }'
. $srcdir/diag.sh assert-content-missing 'quux: { "key": "str4", "value": "jkl3" }'
. $srcdir/diag.sh content-check 'new: jkl3'
. $srcdir/diag.sh assert-content-missing 'deleted: ghi2'
. $srcdir/diag.sh content-check 'quux: { "key": "obj", "value": { "bar": { "k1": "important_msg", "k2": "other_msg" } } }'
. $srcdir/diag.sh custom-content-check 'corge: key: bar val: { "k1": "important_msg", "k2": "other_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh custom-content-check 'prefixed_corge: { "key": "bar", "value": { "k1": "important_msg", "k2": "other_msg" } }' 'rsyslog.out.prefixed.log'
. $srcdir/diag.sh content-check 'garply: k1=important_msg, k2=other_msg'
. $srcdir/diag.sh exit
