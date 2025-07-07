#!/bin/bash
# test trailing LF handling in imuxsock
# note: we use the system socket, but assign a different name to
# it. This is not 100% the same thing as running as root, but it
# is pretty close to it. -- rgerhards, 201602-19
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock"
       SysSock.name="'$RSYSLOG_DYNNAME'-testbench_socket")

$template outfmt,"%msg:%\n"
*.=notice      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
logger -d -u $RSYSLOG_DYNNAME-testbench_socket test
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
exit_test
