#!/bin/bash 
# This file is part of the rsyslog project, released under ASL 2.0 

# we libmaxmindb, in packaged versions, has a small cosmetic memory leak,
# thus we need a supressions file:
export RS_TESTBENCH_VALGRIND_EXTRA_OPTS="$RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=libmaxmindb.supp"

. $srcdir/diag.sh init
. $srcdir/diag.sh startup-vg mmdb.conf
. $srcdir/diag.sh tcpflood -m 100 -j "202.106.0.20\ "
echo doing shutdown 
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown 
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check '{ "city": "Beijing" }'
. $srcdir/diag.sh exit 
