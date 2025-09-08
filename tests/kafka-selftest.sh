#!/bin/bash
# added 2018-10-26 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
set -v -x
check_command_available kcat
export KEEP_KAFKA_RUNNING="YES"

export TESTMESSAGES=100000

export RANDTOPIC="$(printf '%08x' "$(( (RANDOM<<16) ^ RANDOM ))")"

# Set EXTRA_EXITCHECK to dump kafka/zookeeperlogfiles on failure only.
#export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka
download_kafka
stop_zookeeper
stop_kafka

start_zookeeper
start_kafka
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

printf 'injecting messages via kcat\n'
injectmsg_kcat

# experimental: wait until kcat receives everything

timeoutend=10
timecounter=0

printf 'receiving messages via kcat\n'
while [ $timecounter -lt $timeoutend ]; do
	(( timecounter++ ))

	kcat -b 127.0.0.1:29092 -e -C -o beginning -t $RANDTOPIC -f '%s\n' > $RSYSLOG_OUT_LOG
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
