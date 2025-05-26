#!/bin/bash
# add 2020-02-11 by alorbach, released under ASL 2.0
TEST_BYTES_EXPECTED=262152

. ${srcdir:=.}/diag.sh init
./have_relpSrvSetOversizeMode
if [ $? -eq 1 ]; then
  echo "imrelp parameter oversizeMode not available. Test stopped"
  exit 77
fi;
generate_conf
add_conf '
global(
	workDirectory="'$RSYSLOG_DYNNAME.spool'"
	maxMessageSize="256k"
)
module(load="../plugins/imrelp/.libs/imrelp")
input(
	type="imrelp"
	name="imrelp"
	port="'$TCPFLOOD_PORT'"
	ruleset="print"
	MaxDataSize="260k"
)
#input(type="imrelp" port="'$TCPFLOOD_PORT'" maxdatasize="200" oversizeMode="accept")

template(name="print_message" type="list"){
	constant(value="inputname: ")
	property(name="inputname")
	constant(value=", strlen: ")
	property(name="$!strlen")
	constant(value=", message: ")
	property(name="msg")
	constant(value="\n")
}
ruleset(name="print") {
	set $!strlen = strlen($msg);
	action(
		type="omfile" template="print_message"
		file=`echo $RSYSLOG_OUT_LOG`
	)
#	action(
#		type="omstdout"
#		template="print_message"
#	)
}

#template(name="outfmt" type="string" string="%msg%\n")
#:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
#				 file=`echo $RSYSLOG_OUT_LOG`)

'
startup
tcpflood -Trelp-plain -p'$TCPFLOOD_PORT' -m1 -d 262144
# would also works well: 
#	tcpflood -Trelp-plain -p'$TCPFLOOD_PORT' -R 1 -I "imrelp-bigmessage.log"
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

# We need the ^-sign to symbolize the beginning and the $-sign to symbolize the end
# because otherwise we won't know if it was truncated at the right length.

content_check "inputname: imrelp, strlen: 262107, message:  msgnum:00000000:262144:"
count=$(wc -c < $RSYSLOG_OUT_LOG)
if [ $count -lt $TEST_BYTES_EXPECTED ]; then
	echo
	echo "FAIL: expected bytes count $count did not match $TEST_BYTES_EXPECTED. "
	echo
	echo "First 100 bytes of $RSYSLOG_OUT_LOG are: "
	head -c 100 $RSYSLOG_OUT_LOG
	echo 
	echo "Last 100 bytes of $RSYSLOG_OUT_LOG are: "
	tail -c 100 $RSYSLOG_OUT_LOG
	error_exit 1
else
	echo "Found $count bytes in $RSYSLOG_OUT_LOG"
fi

exit_test
