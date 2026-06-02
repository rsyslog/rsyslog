#!/bin/bash
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "trusted property annotation depends on Linux credentials/procfs behavior"

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
global(maxMessageSize="8k")
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock"
      Socket="'$RSYSLOG_DYNNAME'-testbench_socket"
      Annotate="on"
      ParseTrusted="off")

template(name="outfmt" type="string" string="%rawmsg%\n")
*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
long_arg="$(printf 'quoted\\\\value\\"%.0s' {1..300})"
./syslog_caller -m1 -u "$RSYSLOG_DYNNAME-testbench_socket" -L 3900 -w 1000 -- "$long_arg"

shutdown_when_empty
wait_shutdown

content_check "_PID="
content_check "_CMDLINE=\""
content_check "quoted"
exit_test
