#!/bin/bash
# added 2016-03-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[json_object_sucide_in_loop-vg.sh\]: basic test for looping over json object and unsetting it while inside the loop-body
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="corge" type="string" string="corge: key: %$.corge!key% val: %$.corge!value%\n")
template(name="quux" type="string" string="quux: %$.quux%\n")
template(name="post_suicide_foo" type="string" string="post_suicide_foo: '
add_conf "'%\$!foo%'"
add_conf '\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

action(type="mmjsonparse")
set $.garply = "";

foreach ($.quux in $!foo) do {
  if ($.quux!key == "str2") then {
    set $.quux!random_key = $.quux!key;
		unset $!foo; #because it is deep copied, the foreach loop will continue to work, but the action to print "post_sucide_foo" will not see $!foo
	}
  action(type="omfile" file="./rsyslog.out.log" template="quux")
  foreach ($.corge in $.quux!value) do {
    action(type="omfile" file="./rsyslog.out.async.log" template="corge" queue.type="linkedlist" action.copyMsg="on")
  }
}
action(type="omfile" file="./rsyslog.out.log" template="post_suicide_foo")
'
startup_vg
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/json_object_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check 'quux: { "key": "str1", "value": "abc0" }'
. $srcdir/diag.sh content-check 'quux: { "key": "str2", "value": "def1", "random_key": "str2" }'
. $srcdir/diag.sh content-check 'quux: { "key": "str3", "value": "ghi2" }'
. $srcdir/diag.sh assert-content-missing 'quux: { "key": "str4", "value": "jkl3" }'
. $srcdir/diag.sh content-check 'quux: { "key": "obj", "value": { "bar": { "k1": "important_msg", "k2": "other_msg" } } }'
. $srcdir/diag.sh custom-content-check 'corge: key: bar val: { "k1": "important_msg", "k2": "other_msg" }' 'rsyslog.out.async.log'
. $srcdir/diag.sh content-check "post_suicide_foo: ''"
exit_test
