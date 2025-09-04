#!/bin/bash
# added 2018-08-29 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=1000
# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka

export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)

download_kafka
stop_zookeeper
stop_kafka
start_zookeeper
start_kafka

export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")

module(load="../plugins/imkafka/.libs/imkafka")
/* Polls messages from kafka server!*/
input(	type="imkafka"
	topic="'$RANDTOPIC'"
	broker="localhost:29092"
	consumergroup="default"
	confParam=[ "does.not.exist=none",
		"session.timeout.ms=10000",
		"socket.timeout.ms=5000",
		"socket.keepalive.enable=true",
		"reconnect.backoff.jitter.ms=1000",
		"enable.partition.eof=false" ]
	)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

action( type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup

# We inject messages, even though we know this will not work. The reason
# is that we want to ensure we do not get a segfault in such an error case
injectmsg_kcat

shutdown_when_empty
wait_shutdown

content_check "error setting custom configuration parameter 'does.not.exist=none'"
delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

exit_test
