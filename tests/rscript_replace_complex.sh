#!/bin/bash
# added 2014-10-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_replace_complex.sh\]: a more complex test for replace script-function
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$.replaced_msg%\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

set $.replaced_msg = replace($msg, "syslog", "rsyslog");
set $.replaced_msg = replace($.replaced_msg, "hello", "hello_world");
set $.replaced_msg = replace($.replaced_msg, "foo_bar_baz", "FBB");
set $.replaced_msg = replace($.replaced_msg, "as_longer_this_string_as_more_probability_to_catch_the_bug", "ss");

action(type="omfile" file="./rsyslog.out.log" template="outfmt")
'
startup
. $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/complex_replace_input
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
. $srcdir/diag.sh content-check "try to replace rsyslog and syrsyslog with rrsyslog"
. $srcdir/diag.sh content-check "try to replace hello_world in hello_worldlo and helhello_world with hello_world_world"
. $srcdir/diag.sh content-check "try to FBB in FBB_quux and quux_FBB with FBB"
. $srcdir/diag.sh content-check "in the end of msg; try to not lose as_longer_this_string_as_more_probability_to_catch_the_bu"
exit_test
