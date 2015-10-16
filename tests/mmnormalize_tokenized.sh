#!/bin/bash
# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_tokenized.sh\]: test for mmnormalize tokenized field_type
. $srcdir/diag.sh init
. $srcdir/diag.sh startup mmnormalize_tokenized.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/tokenized_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
cp rsyslog.out.log /tmp/
. $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "90.100.110.120", "130.140.150.160" ]'
. $srcdir/diag.sh content-check '[ "192.168.1.2", "192.168.1.3", "192.168.1.4" ]'
. $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "190.200.210.220" ]'
. $srcdir/diag.sh content-check '[ "\/bin", "\/usr\/local\/bin", "\/usr\/bin" ] foo'
. $srcdir/diag.sh content-check '[ [ [ "10" ] ], [ [ "20" ], [ "30", "40", "50" ], [ "60", "70", "80" ] ], [ [ "90" ], [ "100" ] ] ]'
. $srcdir/diag.sh exit
