#!/bin/bash
# This tests legacy syntax.
# Added 2019-08-13 by Rainer Gerhards; released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_logger_has_option_d
generate_conf
add_conf '
$ModLoad ../plugins/imuxsock/.libs/imuxsock
$OmitLocalLogging on

$AddUnixListenSocket '$RSYSLOG_DYNNAME'-testbench_socket

template(name="outfmt" type="string" string="%msg:%\n")
*.=notice      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
# send a message with trailing LF
logger -d -u $RSYSLOG_DYNNAME-testbench_socket test
shutdown_when_empty
wait_shutdown
export EXPECTED=" test"
cmp_exact
exit_test
