#!/bin/bash
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket")

template(name="outfmt" type="string" string="%msg:%\n")
*.=notice      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
logger -d -u $RSYSLOG_DYNNAME-testbench_socket test
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
exit_test
