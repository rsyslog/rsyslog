#!/bin/bash
## @file omkafka-headers.sh
## @brief Verify Kafka headers are produced by omkafka.
# added 2024-05-05 by AI Assistant
. ${srcdir:=.}/diag.sh init
check_command_available kafkacat

export KEEP_KAFKA_RUNNING="YES"
export RANDTOPIC=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 8 | head -n 1)
export EXTRA_EXITCHECK=dumpkafkalogs
export EXTRA_EXIT=kafka

download_kafka
stop_zookeeper
stop_kafka

start_zookeeper
start_kafka
create_kafka_topic $RANDTOPIC '.dep_wrk' '22181'

generate_conf
add_conf '
main_queue(queue.timeoutactioncompletion="60000" queue.timeoutshutdown="60000")
$imdiagInjectDelayMode full

module(load="../plugins/omkafka/.libs/omkafka")

template(name="outfmt" type="string" string="%msg%")
local0.* action(
    type="omkafka"
    topic="'$RANDTOPIC'"
    broker="localhost:29092"
    kafkaHeader=["testkey=testvalue"]
    template="outfmt"
)
'

startup
injectmsg 0 1

kafkacat -b localhost:29092 -t $RANDTOPIC -C -c1 -f "%h\n" > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown

grep -q "testkey=testvalue" "$RSYSLOG_OUT_LOG"
exit $?
