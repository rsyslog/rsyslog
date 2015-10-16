#!/bin/bash
# added 2014-11-17 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_regex_disabled.sh\]: test for mmnormalize regex field_type with allow_regex disabled
. $srcdir/diag.sh init
. $srcdir/diag.sh startup mmnormalize_regex_disabled.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/regex_input
echo doing shutdown
. $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh assert-content-missing '192' #several ips in input are 192.168.1.0/24
. $srcdir/diag.sh exit
