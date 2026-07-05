#!/bin/bash
# Test imptcp compression.mode="stream:auto": a single listener must
# accept BOTH a zlib-compressed session and a plain (uncompressed)
# session from independent senders on the same port. This mirrors the
# production scenario where a fleet of omfwd clients migrates from
# plain to stream-compressed delivery one peer at a time.
#
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4000

# ---- receiver (instance 1): imptcp with stream:auto -------------------
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      compression.mode="stream:auto")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
                                 file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
# capture the receiver port before generate_conf 2 reassigns TCPFLOOD_PORT
export RCVR_PORT=$TCPFLOOD_PORT

# ---- sender (instance 2): omfwd with stream:always --------------------
generate_conf 2
add_conf '
module(load="../plugins/imdiag/.libs/imdiag")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.imtcp_port2")

*.* action(type="omfwd"
           target="127.0.0.1" port="'$RCVR_PORT'" protocol="tcp"
           compression.mode="stream:always"
           template="RSYSLOG_TraditionalForwardFormat")
' 2
startup 2

# half of the messages go through the compressing sender (zlib stream)
injectmsg2 0 $((NUMMESSAGES / 2))
shutdown_when_empty 2
wait_shutdown 2

# at this point the receiver has accepted ONE compressed session; now we
# wait for those 2000 lines to land on disk before opening the second
# session - this ensures we exercise auto-detection on a fresh session,
# not on the tail of the previous stream.
wait_file_lines "$RSYSLOG_OUT_LOG" $((NUMMESSAGES / 2))

# second half: a plain TCP session to the SAME listener, no compression.
# The receiver must auto-detect plain framing on this brand-new connection.
tcpflood -p$RCVR_PORT -m$((NUMMESSAGES / 2)) -i$((NUMMESSAGES / 2))

wait_file_lines "$RSYSLOG_OUT_LOG" $NUMMESSAGES
shutdown_when_empty
wait_shutdown

seq_check
exit_test
