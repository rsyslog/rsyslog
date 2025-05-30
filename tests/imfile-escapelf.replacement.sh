#!/bin/bash
# Written in 2019 by Rainer Gerhards
# This is part of the rsyslog testbench, licensed under ASL 2.0
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" ruleset="output" escapelf.replacement="[LF]"
	File="./'$RSYSLOG_DYNNAME'.input" tag="file:" startmsg.regex="^msg")

template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="output") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
# make sure file exists when rsyslog starts up
echo 'msg 1 part 1
 msg 1 part 2
msg 2
msg INVISIBLE by design' > $RSYSLOG_DYNNAME.input
startup
export NUMMESSAGES=2
shutdown_when_empty
wait_shutdown
export EXPECTED='msg 1 part 1[LF] msg 1 part 2
msg 2'
cmp_exact
exit_test
