#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=100
export NUMMESSAGESFULL=$NUMMESSAGES
export WAITTIMEOUT=20

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
# impstats in order to gain insight into error cases
module(load="../plugins/impstats/.libs/impstats"
	log.file="'$RSYSLOG_DYNNAME.pstats'"
	interval="1" log.syslog="off")
$imdiagInjectDelayMode full

# Load mods
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omazureeventhubs/.libs/omazureeventhubs")

# imtcp
input(	type="imtcp" 
	port="0"
	ruleset="default"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# templates
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
template(name="generic" type="list" option.jsonf="on") {
	property(outname="timestamp" name="timereported" dateFormat="rfc3339" format="jsonf")
	constant(value="\"source\": \"EventHubMessage\", ")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="severity" name="syslogseverity" caseConversion="upper" format="jsonf" datatype="number")
	property(outname="facility" name="syslogfacility" format="jsonf" datatype="number")
	property(outname="appname" name="syslogtag" format="jsonf")
	property(outname="message" name="msg" format="jsonf" )
	property(outname="etlsource" name="$myhostname" format="jsonf")}

# ruleset
ruleset(name="default") {
	if $msg contains "msgnum:" then {
		action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
		action(type="omfile" template="generic" file="'$RSYSLOG_OUT_LOG'-generic.log")
		action(name="omazureeventhubs"
			type="omazureeventhubs"
			azurehost="'$AZURE_HOST'"
			azureport="'$AZURE_PORT'"
			azure_key_name="'$AZURE_KEY_NAME'"
			azure_key="'$AZURE_KEY'"
			container="'$AZURE_CONTAINER'"
			template="generic"
			eventproperties=[	"Table=TestTable",
						"Format=JSON"]
		)
	} else {
		action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
	}
	stop
}

'
echo Starting sender instance [omazureeventhubs]
startup

echo Inject messages into rsyslog sender instance  
# injectmsg 1 $NUMMESSAGES
tcpflood -m$NUMMESSAGES -i1

wait_file_lines $RSYSLOG_OUT_LOG $NUMMESSAGESFULL 100

# experimental: wait until kafkacat receives everything
timeoutend=$WAITTIMEOUT
timecounter=0

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
				if [ "$ACCEPTED_MSG" -eq "$NUMMESSAGESFULL" ]; then
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
			fi
		fi
	fi

	$TESTTOOL_DIR/msleep 1000
done
unset count

shutdown_when_empty
wait_shutdown
error_exit 1
