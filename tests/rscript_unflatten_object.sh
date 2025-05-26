#!/bin/bash
# added 2020-08-16 by Julien Thomas, released under ASL 2.0

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

set $!source.ip = "1.2.3.4";
set $!source.bytes = 3258;
set $!source.geo.country_iso_code = "FR";
set $!destination.ip = "4.3.2.1";

set $.unflatten = unflatten($!, ".");
set $.ret = script_error();
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m 3
wait_file_lines "$RSYSLOG_OUT_LOG" 3 60
shutdown_when_empty
wait_shutdown

# this test may need changes to produce a more deterministic
# output by sorting keys
EXPECTED=' msgnum:00000000: 0 { "source": { "ip": "1.2.3.4", "bytes": 3258, "geo": { "country_iso_code": "FR" } }, "destination": { "ip": "4.3.2.1" } }
 msgnum:00000001: 0 { "source": { "ip": "1.2.3.4", "bytes": 3258, "geo": { "country_iso_code": "FR" } }, "destination": { "ip": "4.3.2.1" } }
 msgnum:00000002: 0 { "source": { "ip": "1.2.3.4", "bytes": 3258, "geo": { "country_iso_code": "FR" } }, "destination": { "ip": "4.3.2.1" } }'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
