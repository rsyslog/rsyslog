#!/bin/bash
# added 2016-03-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_object_sucide_in_loop-vg.sh\]: basic test for looping over json object and unsetting it while inside the loop-body
. $srcdir/diag.sh init json_object_suicide_in_loop-vg.sh
. $srcdir/diag.sh startup-vg json_object_suicide_in_loop.conf
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
. $srcdir/diag.sh content-check 'quux: { "key": "obj", "value": { "bar": { "k1": "important_msg", "k2": "other_msg" } } }'
. $srcdir/diag.sh custom-content-check 'corge: key: bar val: { "k1": "important_msg", "k2": "other_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh content-check "post_suicide_foo: ''"
. $srcdir/diag.sh exit
