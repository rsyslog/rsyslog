#!/bin/bash
#
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/improg/.libs/improg")
template(name="outfmt" type="string" string="#%msg%#\n")
input(type="improg" tag="tag" ruleset="ruleset"
	  binary="'$PYTHON' '$srcdir'/improg-multiline-test.py"
	  confirmmessages="off" closetimeout="2000"
	 )
ruleset(name="ruleset") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
shutdown_when_empty
wait_shutdown
NUM_ITEMS=10
echo "expected: $NUM_EXPECTED"
echo "file: $RSYSLOG_OUT_LOG"
content_check_with_count "#multi-line-string#" $NUM_EXPECTED $NUM_ITEMS
exit_test
