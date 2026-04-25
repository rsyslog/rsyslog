#!/bin/bash
# Regression test for regex framing with an oversized maxMessageSize.
# This must fail cleanly when the first TCP session is constructed rather than
# overflowing the regex framing buffer arithmetic.
. ${srcdir:=.}/diag.sh init
if [ "$USE_VALGRIND" == "YES" ] || [ "$USE_VALGRIND" == "YES-NOLEAK" ]; then
	printf 'SKIP: this test captures early startup diagnostics via RS_REDIR, '
	printf 'which startup_vg does not honor\n'
	exit 77
fi
export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"

generate_conf
add_conf '
global(maxMessageSize="2147483647")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp"
      port="0"
      framing.delimiter.regex="^<[0-9]{2}>")
'

startup
content_check 'framing.delimiter.regex requires maxMessageSize <=' "${RSYSLOG_DYNNAME}.rsyslog.log"
shutdown_immediate
wait_shutdown
exit_test
