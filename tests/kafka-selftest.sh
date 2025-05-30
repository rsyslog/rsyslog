#!/bin/bash
# added 2018-10-26 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
check_command_available kafkacat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=100000

export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
#export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka
download_kafka
stop_zookeeper
stop_kafka

start_zookeeper
start_kafka
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

printf 'injecting messages via kafkacat\n'
injectmsg_kafkacat

# experimental: wait until kafkacat receives everything

timeoutend=10
timecounter=0

printf 'receiving messages via kafkacat\n'
while [ $timecounter -lt $timeoutend ]; do
	(( timecounter++ ))

	kafkacat -b localhost:29092 -e -C -o beginning -t $RANDTOPIC -f '%s\n' > $RSYSLOG_OUT_LOG
	count=$(wc -l < ${RSYSLOG_OUT_LOG})
	if [ $count -eq $TESTMESSAGES ]; then
		printf '**** wait-kafka-lines success, have %d lines ****\n\n' "$TESTMESSAGES"
		break
	else
		if [ "x$timecounter" == "x$timeoutend" ]; then
			echo wait-kafka-lines failed, expected $TESTMESSAGES got $count
			error_exit 1
		else
			echo wait-file-lines not yet there, currently $count lines
			printf '\n'
			$TESTTOOL_DIR/msleep 1000
		fi
	fi
done
unset count

#end experimental

delete_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

sed -i 's/ msgnum://' "$RSYSLOG_OUT_LOG"
seq_check 1 $TESTMESSAGES -d

exit_test
