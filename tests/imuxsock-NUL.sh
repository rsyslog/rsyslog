#!/usr/bin/env bash
# Regression test for issue #4941: an imuxsock datagram containing an embedded
# NUL byte must be processed using the received datagram length, sanitized as
# "#000", and written without leaking uninitialized bytes from the receive
# buffer. The oracle is exact raw-message output after synchronized shutdown.
. "${srcdir:=.}/diag.sh" init

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket")

template(name="outfmt" type="string" string="%rawmsg%\n")
if $rawmsg startswith "<1>a" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
./syslog_caller -u "$RSYSLOG_DYNNAME-testbench_socket" -s 1 -m 1 -n
shutdown_when_empty
wait_shutdown
content_check --regex '^<1>a#000b$'
if [ "$(wc -l < "$RSYSLOG_OUT_LOG")" -ne 1 ]; then
  echo "unexpected extra output generated, $RSYSLOG_OUT_LOG is:"
  cat -n "$RSYSLOG_OUT_LOG"
  error_exit 1
fi
exit_test
