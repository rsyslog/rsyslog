#!/bin/bash
# added 2014-11-11 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[json_array_looping.sh\]: basic test for looping over json array
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="garply" type="string" string="garply: %$.garply%\n")
template(name="grault" type="string" string="grault: %$.grault%\n")
template(name="prefixed_grault" type="string" string="prefixed_grault: %$.grault%\n")
template(name="quux" type="string" string="quux: %$.quux%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

action(type="mmjsonparse")
set $.garply = "";

ruleset(name="prefixed_writer" queue.type="linkedlist" queue.workerthreads="5") {
  action(type="omfile" file="./rsyslog.out.prefixed.log" template="prefixed_grault" queue.type="linkedlist")
}

foreach ($.quux in $!foo) do {
  action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="quux")
  foreach ($.corge in $.quux!bar) do {
     reset $.grault = $.corge;
     action(type="omfile" file="./rsyslog.out.async.log" template="grault" queue.type="linkedlist" action.copyMsg="on")
     call prefixed_writer
     if ($.garply != "") then
         set $.garply = $.garply & ", ";
     reset $.garply = $.garply & $.grault!baz;
  }
}
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="garply")
'
startup
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_array_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 
. $srcdir/diag.sh content-check 'quux: abc0'
. $srcdir/diag.sh content-check 'quux: def1'
. $srcdir/diag.sh content-check 'quux: ghi2'
. $srcdir/diag.sh content-check 'quux: { "bar": [ { "baz": "important_msg" }, { "baz": "other_msg" } ] }'
. $srcdir/diag.sh custom-content-check 'grault: { "baz": "important_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh custom-content-check 'grault: { "baz": "other_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh custom-content-check 'prefixed_grault: { "baz": "important_msg" }' 'rsyslog.out.prefixed.log'
. $srcdir/diag.sh custom-content-check 'prefixed_grault: { "baz": "other_msg" }' 'rsyslog.out.prefixed.log'
. $srcdir/diag.sh content-check 'garply: important_msg, other_msg'
exit_test
