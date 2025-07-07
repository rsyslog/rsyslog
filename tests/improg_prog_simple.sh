#!/bin/bash
# add 2019-04-04 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/improg/.libs/improg")
input(type="improg" tag="tag" ruleset="ruleset"
	  binary="'${srcdir:=.}'/improg-simul.sh -e '$RSYSLOG_DYNNAME'.stderr"
	  confirmmessages="off" closetimeout="2000"
	 )
ruleset(name="ruleset") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
shutdown_when_empty
wait_shutdown
content_check "program data"
if [ -e $RSYSLOG_DYNNAME.stderr ]; then 
	echo 'improg-simul stderr:'
	cat $RSYSLOG_DYNNAME.stderr 
	error_exit 1
fi
exit_test
