#!/bin/bash
# Copyright (C) 2015-03-04 by rainer gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(	type="imuxsock" socket="'$RSYSLOG_DYNNAME'-testbench_socket"
	useSpecialParser="off"
	parseHostname="on")

template(name="outfmt" type="string" string="%msg:%\n")
*.=notice      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
logger -d --rfc3164 -u $RSYSLOG_DYNNAME-testbench_socket test
if [ ! $? -eq 0 ]; then
logger -d -u $RSYSLOG_DYNNAME-testbench_socket test
fi;
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
exit_test
