#!/bin/bash
# added 2015-05-22 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[stop_when_array_has_element.sh\]: loop detecting presense of an element and stopping ruleset execution
. $srcdir/diag.sh init stop_when_array_has_element.sh
generate_conf
add_conf '
template(name="foo" type="string" string="%$!foo%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

action(type="mmjsonparse")

foreach ($.quux in $!foo) do {
  if ($.quux == "xyz0") then stop
}
action(type="omfile" file="./rsyslog.out.log" template="foo")
'
startup
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/stop_when_array_has_elem_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-check '"abc0"'
. $srcdir/diag.sh content-check '"abc2"'
. $srcdir/diag.sh assert-content-missing 'xyz0'
exit_test
