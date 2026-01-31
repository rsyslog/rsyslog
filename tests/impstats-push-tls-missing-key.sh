#!/bin/bash
# Test for impstats push TLS validation - missing keyfile
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "Solaris has limitations"

export RS_REDIR="2>$RSYSLOG_DYNNAME.err.log"

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats"
       interval="1"
       format="prometheus"
       log.file="'"$RSYSLOG_OUT_LOG"'"
       resetCounters="off"

       # Push config (TLS validation)
       push.url="https://localhost:8428/api/v1/write"
       push.tls.certfile="/etc/ssl/certs/ca-certificates.crt"
)

action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)
'

startup
sleep 1

shutdown_when_empty
wait_shutdown

if ! grep -q "push.tls.certfile is set but push.tls.keyfile is missing" "$RSYSLOG_DYNNAME.err.log"; then
	echo "FAIL: expected TLS missing keyfile error in $RSYSLOG_DYNNAME.err.log"
	cat -n "$RSYSLOG_DYNNAME.err.log" || true
	error_exit 1
fi

exit_test
