#!/bin/bash
# the whole point of this test is just to check that imudp
# does not block rsyslog termination. This test was introduced
# after we had a regression where imudp's worker threads were
# not properly terminated.
# Copyright 2014 by Rainer Gerhards, licensed under ASL 2.0
echo \[imudp_thread_hang\]: a situation where imudp caused a hang
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imudp_thread_hang.conf
./msleep 1000
. $srcdir/diag.sh shutdown-immediate
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh exit
