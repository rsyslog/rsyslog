#!/bin/bash
# added 2014-11-17 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_regex.sh\]: test for mmnormalize regex field_type
. $srcdir/diag.sh init
startup mmnormalize_regex.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/regex_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-check 'host and port list: 192.168.1.2:80, 192.168.1.3, 192.168.1.4:443, 192.168.1.5'
exit_test
