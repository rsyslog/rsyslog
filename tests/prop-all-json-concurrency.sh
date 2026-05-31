#!/bin/bash
# Stress `$!all-json` capture while another action serializes the same message.
# The oracle is the captured string: after `set $.interim = $!all-json`, we
# `unset` the original field and later extract `nbr` back out of the logged
# `$.interim` values. The extracted sequence must still be complete and ordered,
# proving the captured string survived both the concurrent serializer and the
# later unset.
# Added 2015-12-16 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[prop-all-json-concurrency.sh\]: testing $!all-json capture under concurrent serialization
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=500000
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$.interim%\n")
template(name="all-json" type="string" string="%$!%\n")

if $msg contains "msgnum:" then {
	set $!tree!here!nbr = field($msg, 58, 2);
	action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG` template="all-json"
	       queue.type="linkedList")

	set $.interim = $!all-json;
	unset $!tree!here!nbr;
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt"
	       queue.type="fixedArray")
}
'
startup
sleep 1
tcpflood -m$NUMMESSAGES
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
extracted_file="${RSYSLOG_DYNNAME}.captured-nbr.log"
sed -n 's/^.*"nbr": "\([0-9][0-9]*\)".*$/\1/p' "$RSYSLOG_OUT_LOG" > "$extracted_file"
linecount=$(wc -l < "$extracted_file")
if [ "$linecount" -ne "$NUMMESSAGES" ]; then
	printf 'FAIL: expected %s extracted numbers, got %s\n' "$NUMMESSAGES" "$linecount"
	printf 'First captured lines:\n'
	sed -n '1,10p' "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
SEQ_CHECK_FILE="$extracted_file" seq_check
exit_test
