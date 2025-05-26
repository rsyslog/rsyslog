#!/bin/bash
# added 2018-10-22 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT=5000 # 5sec
generate_conf
add_conf '
main_queue(queue.type="direct")

module(load="../plugins/imkafka/.libs/imkafka")

input(	type="imkafka"
	ruleset="kafka"
	topic="irrelevant"
	broker="localhost:29092"
	consumergroup="default"
	confParam=[ "socket.timeout.ms=5000",
		"socket.keepalive.enable=true" ]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="kafka") {
	continue
}

if prifilt("local4.*") then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt" )
else
	action( type="omfile" file="'$RSYSLOG_OUT_LOG.syslog.log'" template="outfmt" )
'

export RSTB_DAEMONIZE="YES"
startup
injectmsg 0 10000
shutdown_when_empty
wait_shutdown
seq_check 0 9999
exit_test
