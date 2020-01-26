#!/bin/bash
# test trailing LF handling in imuxsock
. ${srcdir:=.}/diag.sh init
./syslog_caller -fsyslog_inject-l -m0 > /dev/null 2>&1
no_liblogging_stdlog=$?
if [ $no_liblogging_stdlog -ne 0 ];then
  echo "liblogging-stdlog not available - skipping test"
  exit 77
fi

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket")

template(name="outfmt" type="string" string="%msg:%\n")
local1.*    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
# send a message with trailing LF
./syslog_caller -fsyslog_inject-c -m1 -C "uxsock:$RSYSLOG_DYNNAME-testbench_socket"
shutdown_when_empty
wait_shutdown
export EXPECTED=" test 1#0112"
cmp_exact
exit_test
