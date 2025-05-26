#!/bin/bash
# a test to check load balancing via global variables
# note: for simplicity, we use omfile output; in practice this will usually
# be some kind of network output, e.g. omfwd or omrelp. From the method's
# point of view, the actual output does not matter.
# added by Rainer Gerhards 2020-01-03
# part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000 # sufficient for our needs
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

# note: do NOT initialize $/lbcntr - it starts at "", which becomes 0 after cnum()
if $msg contains "msgnum" then {
	set $.actnbr = cnum($/lbcntr) % 4;
	if $.actnbr == 0 then {
		action(type="omfile" file="'$RSYSLOG_DYNNAME'0.log" template="outfmt")
	} else if $.actnbr == 1 then {
		action(type="omfile" file="'$RSYSLOG_DYNNAME'1.log" template="outfmt")
	} else if $.actnbr == 2 then {
		action(type="omfile" file="'$RSYSLOG_DYNNAME'2.log" template="outfmt")
	} else {
		action(type="omfile" file="'$RSYSLOG_DYNNAME'3.log" template="outfmt")
	}
	set $/lbcntr = cnum($/lbcntr) + 1;
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check # validate test result as such
export SEQ_CHECK_FILE="${RSYSLOG_DYNNAME}0.log"
seq_check 0 $((NUMMESSAGES -4)) -i 4
printf 'Checking file 1\n'
export SEQ_CHECK_FILE="${RSYSLOG_DYNNAME}1.log"
printf 'Checking file 2\n'
export SEQ_CHECK_FILE="${RSYSLOG_DYNNAME}2.log"
printf 'Checking file 3\n'
export SEQ_CHECK_FILE="${RSYSLOG_DYNNAME}3.log"
exit_test
