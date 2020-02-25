#!/bin/bash
# This test injects a message and checks if it is received by
# imjournal. We use a special test string which we do not expect
# to be present in the regular log stream. So we do not expect that
# any other journal content matches our test message. We skip the 
# test in case message does not make it even to journal which may 
# sometimes happen in some environments.
# addd 2017-10-25 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	# we turn off rate-limiting, else we may miss our test message:
	RateLimit.interval="0"
       )

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
'
TESTMSG="TestBenCH-RSYSLog imjournal This is a test message - $(date +%s) - $RSYSLOG_DYNNAME"
./journal_print "$TESTMSG"
if [ $? -ne 0 ]; then
        echo "SKIP: failed to put test into journal."
        error_exit 77
fi

# sleep 1 to get this test to reliably detect the message
sleep 1

journalctl -an 200 > /dev/null 2>&1
if [ $? -ne 0 ]; then
        echo "SKIP: cannot read journal."
        error_exit 77
fi

journalctl -an 200 | grep -qF "$TESTMSG"
if [ $? -ne 0 ]; then
        echo "SKIP: cannot find '$TESTMSG' in journal."
        error_exit 77
fi

# do first run to process all the stuff already in journal db
startup

# give the journal ~5 minutes to forward the message, see
# https://github.com/rsyslog/rsyslog/issues/2564#issuecomment-435849660
content_check_with_count "$TESTMSG" 1 300

shutdown_when_empty
wait_shutdown

printf '%s first rsyslogd run done, now restarting\n' "$(tb_timestamp)"

#now do a second which should NOT capture testmsg again
# craft new testmessage as shutdown condition:
TESTMSG2="TestBenCH-RSYSLog imjournal This is a test message 2 - $(date +%s) - $RSYSLOG_DYNNAME"
startup
./journal_print "$TESTMSG2"
content_check_with_count "$TESTMSG2" 1 300
shutdown_when_empty
wait_shutdown

printf '%s both rsyslogd runs finished, doing final result check\n' "$(tb_timestamp)"

# now check the original one is there
content_count_check "$TESTMSG" 1
exit_test
