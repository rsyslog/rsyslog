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

template(name="hostname" type="string" string="%hostname%")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
action(type="omsendertrack" senderid="hostname" statefile="'$RSYSLOG_DYNNAME'.sendertrack")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg_literal '<167>Mar  1 01:00:00 sender1.example.net tag msgnum:00000000:'
injectmsg_literal '<167>Mar  1 01:00:00 sender1.example.net tag msgnum:00000001:'
shutdown_when_empty
wait_shutdown

grep '"sender":"sender1.example.net"' $RSYSLOG_DYNNAME.sendertrack
grep '"messages":7' $RSYSLOG_DYNNAME.sendertrack

exit_test
