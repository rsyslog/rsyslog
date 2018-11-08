#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
#export USE_VALGRIND="YES" # to enable this to run under valgrind
export ES_PORT=19200
download_elasticsearch
prepare_elasticsearch
# change settings to cause bulk rejection errors
cat >> $dep_work_dir/es/config/elasticsearch.yml <<EOF
thread_pool.bulk.queue_size: 1
thread_pool.bulk.size: 1
EOF
start_elasticsearch

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" interval="1"
	   log.file="'$RSYSLOG_DYNNAME'.spool/es-stats.log" log.syslog="off" format="cee")

set $.msgnum = field($msg, 58, 2);
set $.testval = cnum($.msgnum % 2);
if $.testval == 0 then {
	# these should be successful
	set $!msgnum = $.msgnum;
} else {
	# these should cause "hard" errors
	set $!msgnum = "x" & $.msgnum;
}

template(name="tpl" type="string"
    string="{\"msgnum\":\"%$!msgnum%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="id-template" type="string" string="%$.es_msg_id%")

ruleset(name="error_es") {
	action(type="omfile" template="RSYSLOG_DebugFormat" file="'$RSYSLOG_DYNNAME'.spool/es-bulk-errors.log")
}

ruleset(name="try_es") {
	if strlen($.omes!status) > 0 then {
		# retry case
		if ($.omes!status == 200) or ($.omes!status == 201) or (($.omes!status == 409) and ($.omes!writeoperation == "create")) then {
			stop # successful
		}
		if ($.omes!writeoperation == "unknown") or (strlen($.omes!error!type) == 0) or (strlen($.omes!error!reason) == 0) then {
			call error_es
			stop
		}
		if ($.omes!status == 400) or ($.omes!status < 200) then {
			call error_es
			stop
		}
		# else fall through to retry operation
	}
	if strlen($.omes!_id) > 0 then {
		set $.es_msg_id = $.omes!_id;
	} else {
		# NOTE: in production code, use $uuid - depends on rsyslog being compiled with --enable-uuid
		set $.es_msg_id = $.msgnum;
	}
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="'${ES_PORT:-19200}'"
	       template="tpl"
	       writeoperation="create"
	       bulkid="id-template"
	       dynbulkid="on"
	       bulkmode="on"
	       retryfailures="on"
	       retryruleset="try_es"
	       searchType="test-type"
	       searchIndex="rsyslog_testbench")
}

if $msg contains "msgnum:" then {
	call try_es
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

curl -s -H 'Content-Type: application/json' -XPUT localhost:${ES_PORT:-19200}/rsyslog_testbench/ -d '{
  "mappings": {
    "test-type": {
      "properties": {
        "msgnum": {
          "type": "integer"
        }
      }
    }
  }
}
' | python -mjson.tool
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="debug.log"
startup
if [ -n "${USE_GDB:-}" ] ; then
	echo attach gdb here
	sleep 54321 || :
fi
numrecords=100
success=50
badarg=50
injectmsg 0 $numrecords
sleep 2
# TODO: wait-grep for $success line appearing in pstats log
shutdown_when_empty
wait_shutdown
es_getdata $numrecords $ES_PORT
rc=$?

stop_elasticsearch
cleanup_elasticsearch

if [ -f $RSYSLOG_DYNNAME.work ] ; then
	< $RSYSLOG_DYNNAME.work  \
	python -c '
import sys,json
records = int(sys.argv[1])
extra_recs = open(sys.argv[2], "w")
missing_recs = open(sys.argv[3], "w")
expectedrecs = {}
rc = 0
nextra = 0
nmissing = 0
for ii in xrange(0, records*2, 2):
	ss = "{:08}".format(ii)
	expectedrecs[ss] = ss
for item in json.load(sys.stdin)["hits"]["hits"]:
	msgnum = item["_source"]["msgnum"]
	if msgnum in expectedrecs:
		del expectedrecs[msgnum]
	else:
		extra_recs.write("FAIL: found unexpected msgnum {} in record\n".format(msgnum))
		nextra += 1
for item in expectedrecs:
	missing_recs.write("FAIL: msgnum {} was missing in Elasticsearch\n".format(item))
	nmissing += 1
if nextra > 0:
	print("FAIL: Found {} unexpected records - see {} for the full list.".format(nextra, sys.argv[2]))
	rc = 1
if nmissing > 0:
	print("FAIL: Found {} missing records - see {} for the full list.".format(nmissing, sys.argv[3]))
	rc = 1
sys.exit(rc)
' $success ${RSYSLOG_DYNNAME}.spool/extra_records ${RSYSLOG_DYNNAME}.spool/missing_records || { rc=$?; errmsg="FAIL: found unexpected or missing records in Elasticsearch"; }
	if [ $rc = 0 ] ; then
		echo "good - no missing or unexpected records were found in Elasticsearch"
	fi
else
	errmsg="FAIL: elasticsearch output file $RSYSLOG_DYNNAME.work not found"
	rc=1
fi

if [ -f ${RSYSLOG_DYNNAME}.spool/es-stats.log ] ; then
	python < ${RSYSLOG_DYNNAME}.spool/es-stats.log -c '
import sys,json
success = int(sys.argv[1])
badarg = int(sys.argv[2])
lasthsh = {}
rc = 0
for line in sys.stdin:
	jstart = line.find("{")
	if jstart >= 0:
		hsh = json.loads(line[jstart:])
		if hsh["name"] == "omelasticsearch":
			lasthsh = hsh
actualsuccess = lasthsh["response.success"]
actualbadarg = lasthsh["response.badargument"]
actualrej = lasthsh["response.bulkrejection"]
actualsubmitted = lasthsh["submitted"]
if actualsuccess != success:
	print("FAIL: expected {} successful responses but omelasticsearch stats reported {}".format(success, actualsuccess))
	rc = 1
if actualbadarg != badarg:
	print("FAIL: expected {} bad argument errors but omelasticsearch stats reported {}".format(badarg, actualbadarg))
	rc = 1
if actualrej == 0:
	print("FAIL: there were no bulk index rejections reported by Elasticsearch")
	rc = 1
if actualsuccess + actualbadarg + actualrej != actualsubmitted:
	print("FAIL: The sum of the number of successful responses and bad argument errors and bulk index rejections {} did not equal the number of requests actually submitted to Elasticsearch {}".format(actualsuccess + actualbadarg + actualrej, actualsubmitted))
	rc = 1
sys.exit(rc)
' $success $badarg || { rc=$?; errmsg="FAIL: expected responses not found in ${RSYSLOG_DYNNAME}.spool/es-stats.log"; }
	if [ $rc = 0 ] ; then
		echo "good - all expected stats were found in Elasticsearch stats file ${RSYSLOG_DYNNAME}.spool/es-stats.log"
	fi
else
	errmsg="FAIL: stats file ${RSYSLOG_DYNNAME}.spool/es-stats.log not found"
	rc=1
fi

if [ -f ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log ] ; then
	found=0
	for ii in $(seq --format="x%08.f" 1 2 $(expr 2 \* $badarg)) ; do
		if grep -q '^[$][!]:{.*"msgnum": "'$ii'"' ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log ; then
			(( found++ ))
		else
			errmsg="FAIL: missing message $ii in ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log"
			rc=1
		fi
	done
	if [ $found -ne $badarg ] ; then
		errmsg="FAIL: found only $found of $badarg messages in ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log"
		rc=1
	fi
	if grep -q '^[$][.]:{.*"omes": {' ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log ; then
		:
	else
		errmsg="FAIL: es response info not found in ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log"
		rc=1
	fi
	if grep -q '^[$][.]:{.*"status": 400' ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log ; then
		:
	else
		errmsg="FAIL: status 400 not found in ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log"
		rc=1
	fi
else
	errmsg="FAIL: bulk error file ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log not found"
	rc=1
fi

if [ $rc -eq 0 ] ; then
	echo tests completed successfully
else
	cat $RSYSLOG_OUT_LOG
	if [ -f ${RSYSLOG_DYNNAME}.spool/es-stats.log ] ; then
		cat ${RSYSLOG_DYNNAME}.spool/es-stats.log
	fi
	if [ -f ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log ] ; then
		cat ${RSYSLOG_DYNNAME}.spool/es-bulk-errors.log
	fi
	printf '\n%s\n' "$errmsg"
	error_exit 1
fi

exit_test
