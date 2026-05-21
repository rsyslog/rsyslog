#!/bin/bash
# Verify imuxsock rejects contradictory socket ownership settings and reports a
# clear runtime diagnostic for stale sockets with Unlink="off". The stale-socket
# oracle is the default testbench omfile action for rsyslogd internal messages;
# shutdown is synchronized before checking it so the test proves the diagnostic
# reached the configured output destination.
. ${srcdir:=.}/diag.sh init

# This intentionally exercises a module activation failure; the current failure
# path can leave setup allocations for process teardown, so keep this diagnostic
# test focused on the emitted guidance.
export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

invalid_sockpath="$RSYSLOG_DYNNAME-invalid/log"

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock"
      Socket="'$invalid_sockpath'"
      CreatePath="on"
      Unlink="off")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"$RSYSLOG_DYNNAME.invalid.log" 2>&1
if [ $? -eq 0 ]; then
	echo "FAIL: expected imuxsock to reject CreatePath=on with Unlink=off"
	cat "$RSYSLOG_DYNNAME.invalid.log"
	error_exit 1
fi

grep -F 'CreatePath="on" cannot be combined with Unlink="off"' \
	"$RSYSLOG_DYNNAME.invalid.log" >/dev/null || {
	echo "FAIL: expected contradictory imuxsock config diagnostic"
	cat "$RSYSLOG_DYNNAME.invalid.log"
	error_exit 1
}

sockdir="$RSYSLOG_DYNNAME-imuxsock-dir"
sockpath="$sockdir/log"
mkdir "$sockdir"

$PYTHON - "$sockpath" <<'PY'
import socket
import sys

sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
sock.bind(sys.argv[1])
sock.close()
PY

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock"
      Socket="'$sockpath'"
      Unlink="off")
'

startup
shutdown_when_empty
wait_shutdown

grep -F 'socket path already exists and Unlink="off" prevents rsyslog from replacing it' \
	"$RSYSLOG_DYNNAME.started" >/dev/null || {
	echo "FAIL: expected stale imuxsock path diagnostic"
	cat "$RSYSLOG_DYNNAME.started"
	error_exit 1
}

exit_test
