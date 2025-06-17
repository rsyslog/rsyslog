#!/bin/bash
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

cat > $RSYSLOG_DYNNAME.sendertrack <<'JSON'
[
 {"sender":"sender1.example.net","messages":5,"firstseen":1600000000,"lastseen":1600000000}
]
JSON

generate_conf
add_conf '
module(load="../plugins/omsendertrack/.libs/omsendertrack")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="hostname" type="string" string="%hostname%")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
action(type="omsendertrack" template="hostname" statefile="'$RSYSLOG_DYNNAME'.sendertrack")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
tcpflood -h sender1.example.net -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown

grep '"sender":"sender1.example.net"' $RSYSLOG_DYNNAME.sendertrack
grep '"messages":7' $RSYSLOG_DYNNAME.sendertrack

exit_test
