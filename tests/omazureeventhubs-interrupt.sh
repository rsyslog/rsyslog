#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo This test must be run as root [raw socket access required]
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
. ${srcdir:=.}/diag.sh init

# ---	If test is needed, create helper script to store environment variables for 
#	�venthubs access:
#	export AZURE_HOST=""
#	export AZURE_PORT=""
#	export AZURE_KEY_NAME=""
#	export AZURE_KEY=""
#	export AZURE_CONTAINER=""
# ---
source omazureeventhubs-env.sh

export NUMMESSAGES=10000
export NUMMESSAGESFULL=$NUMMESSAGES
export WAITTIMEOUT=60

export QUEUESIZE=100000
export DEQUEUESIZE=64
export DEQUEUESIZEMIN=32
export TESTWORKERTHREADS=3

export interrupt_host="$AZURE_HOST"
export interrupt_port="$AZURE_PORT"
export interrupt_tick="10"

TEST_TIMEOUT_WAIT=180

# REQUIRES EXTERNAL ENVIRONMENT VARIABLES
if [[ -z "${AZURE_HOST}" ]]; then
	echo "SKIP: AZURE_HOST environment variable not SET! Example: <yourname>.servicebus.windows.net - SKIPPING"
	exit 77
fi
if [[ -z "${AZURE_PORT}" ]]; then
	echo "SKIP: AZURE_PORT environment variable not SET! Example: 5671 - SKIPPING"
	exit 77
fi
if [[ -z "${AZURE_KEY_NAME}" ]]; then
	echo "SKIP: AZURE_KEY_NAME environment variable not SET! Example: <yourkeyname> - SKIPPING"
	exit 77
fi
if [[ -z "${AZURE_KEY}" ]]; then
	echo "SKIP: AZURE_KEY environment variable not SET! Example: <yourlongkey> - SKIPPING"
	exit 77
fi
if [[ -z "${AZURE_CONTAINER}" ]]; then
	echo "SKIP: AZURE_CONTAINER environment variable not SET! Example: <youreventhubsname> - SKIPPING"
	exit 77
fi

export AMQPS_ADRESS="amqps://$AZURE_KEY_NAME:$AZURE_KEY@$AZURE_HOST:$AZURE_PORT/$AZURE_NAME"
export AZURE_ENDPOINT="Endpoint=sb://$AZURE_HOST/;SharedAccessKeyName=$AZURE_KEY_NAME;SharedAccessKey=$AZURE_KEY;EntityPath=$AZURE_NAME"

# --- Create/Start omazureeventhubs sender config 

generate_conf
add_conf '
global(
#	debug.whitelist="on"
#	debug.files=["omazureeventhubs.c", "modules.c", "errmsg.c", "action.c", "queue.c", "ruleset.c"]
)

# impstats in order to gain insight into error cases
module(load="../plugins/impstats/.libs/impstats"
	log.file="'$RSYSLOG_DYNNAME.pstats'"
	interval="1" log.syslog="off")
$imdiagInjectDelayMode full

# Load mods
module(load="../plugins/omazureeventhubs/.libs/omazureeventhubs")

# templates
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

local4.* {
	action(	name="omazureeventhubs"
	type="omazureeventhubs"
	azurehost="'$AZURE_HOST'"
	azureport="'$AZURE_PORT'"
	azure_key_name="'$AZURE_KEY_NAME'"
	azure_key="'$AZURE_KEY'"
	container="'$AZURE_CONTAINER'"
#	amqp_address="amqps://'$AZURE_KEY_NAME':'$AZURE_KEY'@'$AZURE_HOST'/'$AZURE_NAME'"
	template="outfmt"
	queue.type="FixedArray"
	queue.size="'$QUEUESIZE'"
	queue.saveonshutdown="on"
	queue.dequeueBatchSize="'$DEQUEUESIZE'"
	queue.minDequeueBatchSize="'$DEQUEUESIZEMIN'"
	queue.minDequeueBatchSize.timeout="1000" # 1 sec
	queue.workerThreads="'$TESTWORKERTHREADS'"
	queue.workerThreadMinimumMessages="'$DEQUEUESIZEMIN'"
	queue.timeoutWorkerthreadShutdown="60000"
	queue.timeoutEnqueue="2000"
	queue.timeoutshutdown="1000"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
	)

	action( type="omfile" file="'$RSYSLOG_OUT_LOG'")
	stop
}

action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
'
echo Starting sender instance [omazureeventhubs]
startup

echo Inject messages into rsyslog sender instance  
injectmsg 1 $NUMMESSAGES

wait_file_lines --interrupt-connection $interrupt_host $interrupt_port $interrupt_tick $RSYSLOG_OUT_LOG $NUMMESSAGESFULL $TEST_TIMEOUT_WAIT

timeoutend=$WAITTIMEOUT
timecounter=0
lastcurrent_time=0

echo "CHECK $RSYSLOG_DYNNAME.pstats"
while [ $timecounter -lt $timeoutend ]; do
	(( timecounter++ ))

	if [ -f "$RSYSLOG_DYNNAME.pstats" ] ; then
		# Read IMPSTATS for verification
		IMPSTATSLINE=$(cat $RSYSLOG_DYNNAME.pstats | grep "origin\=omazureeventhubs" | tail -1 | cut -d: -f5)
		SUBMITTED_MSG=$(echo $IMPSTATSLINE | grep "submitted" | cut -d" " -f2 | cut -d"=" -f2)
		FAILED_MSG=$(echo $IMPSTATSLINE | grep "failures" | cut -d" " -f3 | cut -d"=" -f2)
		ACCEPTED_MSG=$(echo $IMPSTATSLINE | grep "accepted" | cut -d" " -f4 | cut -d"=" -f2)

		if ! [[ $SUBMITTED_MSG =~ $re ]] ; then
			echo "**** omazureeventhubs WAITING FOR IMPSTATS"
		else
			if [ "$SUBMITTED_MSG" -ge "$NUMMESSAGESFULL" ]; then
				if [ "$ACCEPTED_MSG" -ge "$NUMMESSAGESFULL" ]; then
					echo "**** omazureeventhubs SUCCESS: NUMMESSAGESFULL: $NUMMESSAGESFULL, SUBMITTED_MSG:$SUBMITTED_MSG, ACCEPTED_MSG: $ACCEPTED_MSG, FAILED_MSG: $FAILED_MSG"
					shutdown_when_empty
					wait_shutdown
					#cp $RSYSLOG_DEBUGLOG DEBUGDEBUG.log
					exit_test
				else
					echo "**** omazureeventhubs FAIL: NUMMESSAGESFULL: $NUMMESSAGESFULL, SUBMITTED/WAITING: SUBMITTED_MSG:$SUBMITTED_MSG, ACCEPTED_MSG: $ACCEPTED_MSG, FAILED_MSG: $FAILED_MSG"
				fi
			else
				echo "**** omazureeventhubs WAITING: SUBMITTED_MSG:$SUBMITTED_MSG, ACCEPTED_MSG: $ACCEPTED_MSG, FAILED_MSG: $FAILED_MSG"
				current_time=$(date +%s)
				if [ $interrupt_connection == "YES" ] && [ $current_time -gt $lastcurrent_time ] && [ $((current_time % $interrupt_tick)) -eq 0 ] && [ ${count} -gt 1 ]; then
					# Interrupt Connection - requires root and linux kernel >= 4.9 in order to work!
					echo "**** omazureeventhubs WAITING: Interrupt Connection on ${interrupt_host}:${interrupt_port}"
					sudo ss -K dst ${interrupt_host} dport = ${interrupt_port}
				fi
				lastcurrent_time=$current_time
			fi
		fi
	fi

	$TESTTOOL_DIR/msleep 1000
done
unset count

shutdown_when_empty
wait_shutdown
error_exit 1
