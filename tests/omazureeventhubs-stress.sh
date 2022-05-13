#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=50000
export NUMMESSAGESFULL=$NUMMESSAGES
export WAITTIMEOUT=60

export QUEUESIZE=100000
export DEQUEUESIZE=1000
export DEQUEUESIZEMIN=1000
export TESTWORKERTHREADS=10

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
	debug.whitelist="on"
	debug.files=["omazureeventhubs.c", "modules.c", "errmsg.c", "action.c"]
)
# impstats in order to gain insight into error cases
module(load="../plugins/impstats/.libs/impstats"
	log.file="'$RSYSLOG_DYNNAME.pstats'"
	interval="1" log.syslog="off")
$imdiagInjectDelayMode full

# main_queue(queue.dequeueBatchSize="2048")

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
		template="outfmt"
		queue.type="FixedArray"
		queue.size="'$QUEUESIZE'"
		queue.saveonshutdown="on"
		queue.dequeueBatchSize="'$DEQUEUESIZE'"
		queue.minDequeueBatchSize.timeout="1000" # 1 sec
		queue.workerThreads="'$TESTWORKERTHREADS'"
		queue.workerThreadMinimumMessages="'$DEQUEUESIZEMIN'"
		queue.timeoutWorkerthreadShutdown="10000"
		queue.timeoutshutdown="1000"
		action.resumeInterval="1"
		action.resumeRetryCount="2"
	)

	stop
}

action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
'
echo Starting sender instance [omazureeventhubs]
startup

echo Inject messages into rsyslog sender instance  
injectmsg 1 $NUMMESSAGES

# experimental: wait until kafkacat receives everything
timeoutend=$WAITTIMEOUT
timecounter=0

echo "CHECK $RSYSLOG_DYNNAME.pstats"
LAST_ACCEPTED_MSG=0
while [ $timecounter -lt $timeoutend ]; do
	(( timecounter++ ))

	if [ -f "$RSYSLOG_DYNNAME.pstats" ] ; then
		# Read IMPSTATS for verification
		IMPSTATSLINE=$(cat $RSYSLOG_DYNNAME.pstats | grep "origin\=omazureeventhubs" | tail -1 | cut -d: -f5)
		SUBMITTED_MSG=$(echo $IMPSTATSLINE | grep "submitted" | cut -d" " -f2 | cut -d"=" -f2)
		FAILED_MSG=$(echo $IMPSTATSLINE | grep "failures" | cut -d" " -f3 | cut -d"=" -f2)
		ACCEPTED_MSG=$(echo $IMPSTATSLINE | grep "accepted" | cut -d" " -f4 | cut -d"=" -f2)
		MSG_PER_SEC=$(($ACCEPTED_MSG-$LAST_ACCEPTED_MSG))
		LAST_ACCEPTED_MSG=$ACCEPTED_MSG

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
				echo "**** omazureeventhubs WAITING: SUBMITTED_MSG:$SUBMITTED_MSG, ACCEPTED_MSG: $ACCEPTED_MSG, FAILED_MSG: $FAILED_MSG, MSG_PER_SEC: $MSG_PER_SEC"
			fi
		fi
	fi

	$TESTTOOL_DIR/msleep 1000
done
unset count

shutdown_when_empty
wait_shutdown
error_exit 1
