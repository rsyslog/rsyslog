#!/bin/bash
# Test that array literals in RainerScript evaluate to native JSON arrays.
# Covers: string arrays, numeric arrays, mixed arrays, and concatenation
# with an array on the left or right of & (both must serialise to JSON,
# not collapse to arr[0]).  The oracle uses cmp_exact so any formatting
# or type regression is caught immediately.
# added 2026-05-30 by contributor
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# string-only array
set $!str = ["error", "critical", "ops"];

# numeric array - elements must be JSON integers, not quoted strings
set $!num = [1, 2, 3];

# negative numbers, matching unary minus in scalar expressions
set $!neg = [-1, 0, -42];

# mixed string/number array
set $!mix = ["warn", 42];

# array on LEFT of &: full JSON serialisation, not just arr[0]
set $!cl = ["x", "y"] & "=suffix";

# array on RIGHT of &: full JSON serialisation, not just arr[0]
set $!cr = "prefix=" & ["a", "b"];

template(name="outfmt" type="string"
    string="%$!str%\n%$!num%\n%$!neg%\n%$!mix%\n%$!cl%\n%$!cr%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
export EXPECTED='[ "error", "critical", "ops" ]
[ 1, 2, 3 ]
[ -1, 0, -42 ]
[ "warn", 42 ]
[ "x", "y" ]=suffix
prefix=[ "a", "b" ]'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
