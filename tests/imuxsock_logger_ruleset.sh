#!/bin/bash
# test imuxsock with ruleset definition
# rgerhards, 2016-02-02 released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(	type="imuxsock" socket="'$RSYSLOG_DYNNAME'-testbench_socket"
	useSpecialParser="off"
	ruleset="testruleset"
	parseHostname="on")
template(name="outfmt" type="string" string="%msg:%\n")

ruleset(name="testruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
# send a message with trailing LF
logger -d -u $RSYSLOG_DYNNAME-testbench_socket test
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
exit_test
