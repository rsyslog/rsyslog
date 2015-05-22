# added 2015-05-22 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[stop_when_array_has_element.sh\]: loop detecting presense of an element and stopping ruleset execution
source $srcdir/diag.sh init stop_when_array_has_element.sh
source $srcdir/diag.sh startup stop_when_array_has_element.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/stop_when_array_has_elem_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check '"abc0"'
source $srcdir/diag.sh content-check '"abc2"'
source $srcdir/diag.sh assert-content-missing 'xyz0'
source $srcdir/diag.sh exit
