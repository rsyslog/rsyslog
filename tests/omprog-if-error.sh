#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# This test tests omprog if omprog detects errors in the calling
# interface, namely a missing LF on input messages
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%")

if (prifilt("local4.*") and $msg contains "msgnum:") then {
	action(type="omprog" binary="'$srcdir'/testsuites/omprog-defaults-bin.sh p1 p2 p3"
		template="outfmt" name="omprog_action")
} else {
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsg")
}
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown
cat $RSYSLOG_DYNNAME.othermsg

content_check 'must be terminated with \n' $RSYSLOG_DYNNAME.othermsg

export EXPECTED="Starting with parameters: p1 p2 p3
Next parameter is \"p1\"
Next parameter is \"p2\"
Next parameter is \"p3\"
Received msgnum:00000000:
Received msgnum:00000001:
Received msgnum:00000002:
Received msgnum:00000003:
Received msgnum:00000004:
Received msgnum:00000005:
Received msgnum:00000006:
Received msgnum:00000007:
Received msgnum:00000008:
Received msgnum:00000009:
Terminating normally"

cmp_exact $RSYSLOG_OUT_LOG
exit_test
