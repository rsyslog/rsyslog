#!/bin/bash
# added 2015-05-22 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[stop_when_array_has_element.sh\]: loop detecting presense of an element and stopping ruleset execution
. $srcdir/diag.sh init stop_when_array_has_element.sh
startup stop_when_array_has_element.conf
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/stop_when_array_has_elem_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-check '"abc0"'
. $srcdir/diag.sh content-check '"abc2"'
. $srcdir/diag.sh assert-content-missing 'xyz0'
exit_test
