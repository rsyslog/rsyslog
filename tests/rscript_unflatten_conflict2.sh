#!/bin/bash
# added 2021-03-09 by Julien Thomas, released under ASL 2.0

source "${srcdir:=.}/diag.sh" init
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/fmunflatten/.libs/fmunflatten")
input(type="imtcp" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%msg% %$.ret% %$.unflatten%\n")

if (not($msg contains "msgnum:")) then
    stop

set $.x!a = 21;
set $!a!b = "foo";
set $!a.b = $.x;
set $.unflatten = unflatten($!, ".");
set $.ret = script_error();
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1 60
shutdown_when_empty
wait_shutdown

# this test may need changes to produce a more deterministic
# output by sorting keys
EXPECTED=' msgnum:00000000: 0 { "a": { "b": { "a": 21 } } }'
cmp_exact "$RSYSLOG_OUT_LOG"

EXPECTED='fmunflatten.c: warning: while processing flat key "a.b" at depth #1 (final node), overriding existing value of type string by an object'
if ! grep -F "$EXPECTED" "$RSYSLOG_DEBUGLOG"; then
    echo "GREP FAILED"
    echo "  => FILE: $RSYSLOG_DEBUGLOG"
    echo "  => EXPECTED: $EXPECTED"
    error_exit 1
fi

exit_test
