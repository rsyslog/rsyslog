#!/bin/bash
# added 2021-03-09 by Julien Thomas, released under ASL 2.0

source "${srcdir:=.}/diag.sh" init
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debug"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/fmunflatten/.libs/fmunflatten")
input(type="imtcp" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%msg% %$.ret% %$.unflatten%\n")

if (not($msg contains "msgnum:")) then
    stop

set $!a.b.c = "foobar";
set $.unflatten = unflatten($!, "too many chars");
set $.ret = script_error();
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1 60
shutdown_when_empty
wait_shutdown

EXPECTED=' msgnum:00000000: 1 0'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
