#!/bin/bash
# added 2018-10-26 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=100000
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export RANDTOPIC="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"
#export EXTRA_EXITCHECK=dumpkafkalogs
#export EXTRA_EXIT=kafka

download_kafka
stop_zookeeper
stop_kafka
start_zookeeper
start_kafka

generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka"
	ruleset="does-not-exist"
	topic="'$RANDTOPIC'"
	broker="localhost:29092"
	consumergroup="default"
	confParam=[ "session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
} else {
	action(type="omfile" file="'$RSYSLOG_DYNNAME.errmsg'")
}
'
startup

injectmsg_kcat --wait 1 $TESTMESSAGES -d
shutdown_when_empty
wait_shutdown

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'
stop_kafka
stop_zookeeper

seq_check 1 $TESTMESSAGES -d
content_check "ruleset 'does-not-exist' not found" "$RSYSLOG_DYNNAME.errmsg"

exit_test
