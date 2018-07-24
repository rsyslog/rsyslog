#!/bin/bash
# added 2015-11-24 by portant
# This file is part of the rsyslog project, released under ASL 2.0
echo ===========================================================================================
echo \[json_var_case.sh\]: test for JSON upper and lower case variables, and leading underscores
. $srcdir/diag.sh init
generate_conf
add_conf '
global(variables.casesensitive="on")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

# we must make sure the template contains references to the variables
template(name="outfmt" type="string" string="abc:%$!abc% ABC:%$!ABC% aBc:%$!aBc% _abc:%$!_abc% _ABC:%$!_ABC% _aBc:%$!_aBc%\n" option.casesensitive="on")
template(name="outfmt-all-json" type="string" string="%$!all-json%\n")

action(type="mmjsonparse")
set $!_aBc = "7";
action(type="omfile" file="./rsyslog.out.log" template="outfmt")
if $!_aBc != "7" then
	action(type="omfile" file="./rsyslog2.out.log" template="outfmt-all-json")
'
startup
. $srcdir/diag.sh tcpflood -m 1 -M "\"<167>Nov  6 12:34:56 172.0.0.1 test: @cee: { \\\"abc\\\": \\\"1\\\", \\\"ABC\\\": \\\"2\\\", \\\"aBc\\\": \\\"3\\\", \\\"_abc\\\": \\\"4\\\", \\\"_ABC\\\": \\\"5\\\", \\\"_aBc\\\": \\\"6\\\" }\""
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown
# NOTE: conf file updates _aBc to "7"
. $srcdir/diag.sh content-check  "abc:1 ABC:2 aBc:3 _abc:4 _ABC:5 _aBc:7"
exit_test
