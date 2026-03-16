#!/bin/bash
# added 2024-02-19 by rgerhards. Released under ASL 2.0
: "${srcdir:=.}"

max_attempts=2
attempt=1
result=1

run_attempt() {
	. "$srcdir/diag.sh" init
	generate_conf
	export STATSFILE="$RSYSLOG_DYNNAME.stats"
	add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["xlocalhost", "127.0.0.1"] port="'$TCPFLOOD_PORT'" protocol="tcp")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt"
	       action.ExecOnlyWhenPreviousIsSuspended="on")
}
'
	echo Note: intentionally not started any local TCP receiver!

	startup
	injectmsg 0 5000
	shutdown_when_empty
	wait_shutdown

	if [ "$1" -lt "$max_attempts" ]; then
		seq_check --check-only 0 4999
	else
		seq_check 0 4999
	fi
}

while [ "$attempt" -le "$max_attempts" ]; do
	if [ "$attempt" -gt 1 ]; then
		echo "Retrying omfwd-lb-susp timing-sensitive suspension check (attempt $attempt of $max_attempts)."
	fi

	(run_attempt "$attempt")
	result=$?
	if [ "$result" -eq 0 ]; then
		exit 0
	fi

	attempt=$((attempt + 1))
done

echo "omfwd-lb-susp.sh failed after $max_attempts attempts."
exit "$result"
