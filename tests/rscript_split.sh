#!/bin/bash
# Added 2025-12-19 by 20syldev, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!result%\n")

if $msg contains "msgnum:00000000" then {
    # Basic case: normal split with multi-char separator
    set $!input = "abc@example.com, def@example.com, ghi@example.com";
    set $!result = split($!input, ", ");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000001" then {
    # Edge case: empty input string
    set $!result = split("", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000002" then {
    # Edge case: trailing separator
    set $!result = split("a,b,", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000003" then {
    # Edge case: leading separator
    set $!result = split(",a,b", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000004" then {
    # Edge case: multiple separators together (empty field)
    set $!result = split("a,,b", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000005" then {
    # Edge case: input string is just the separator
    set $!result = split(",", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000006" then {
    # Edge case: input string with no separators
    set $!result = split("abc", ",");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
if $msg contains "msgnum:00000007" then {
    # Edge case: empty separator string (returns empty array)
    set $!result = split("abc", "");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m8
shutdown_when_empty
wait_shutdown

export EXPECTED='[ "abc@example.com", "def@example.com", "ghi@example.com" ]
[ "" ]
[ "a", "b", "" ]
[ "", "a", "b" ]
[ "a", "", "b" ]
[ "", "" ]
[ "abc" ]
[ ]'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
