#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=1000 # 1000 is sufficient, as this test is pretty slow
REBIND_INTERVAL=100 # should be enough to run several times for $NUMMESSAGES

queue_empty_check() {
	es_shutdown_empty_check && \
	content_check --check-only --regex '"name": "omelasticsearch".*"submitted": '$NUMMESSAGES \
		$RSYSLOG_DYNNAME.spool/omelasticsearch-stats.log
}
export QUEUE_EMPTY_CHECK_FUNC=queue_empty_check

ensure_elasticsearch_ready

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/impstats/.libs/impstats" interval="1"
	   log.file="'"$RSYSLOG_DYNNAME.spool"'/omelasticsearch-stats.log" log.syslog="off" format="cee")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
        action(type="omelasticsearch"
               server="127.0.0.1"
               serverport="'$ES_PORT'"
               template="tpl"
               searchType="_doc"
               searchIndex="rsyslog_testbench"
               rebindinterval="'$REBIND_INTERVAL'")
'
startup
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown 

es_getdata $NUMMESSAGES $ES_PORT
seq_check  0 $(( NUMMESSAGES - 1 ))
rc=0
if [ -f ${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log ] ; then
	$PYTHON <${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log  -c '
import sys,json
nrecs = int(sys.argv[1])
nrebinds = nrecs/int(sys.argv[2])-1
expected = { "name": "omelasticsearch", "origin": "omelasticsearch", "submitted": nrecs,
	"failed.http": 0, "failed.httprequests": 0, "failed.checkConn": 0, "failed.es": 0,
	"response.success": 0, "response.bad": 0, "response.duplicate": 0, "response.badargument": 0,
	"response.bulkrejection": 0, "response.other": 0, "rebinds": nrebinds }
actual = {}
for line in sys.stdin:
	jstart = line.find("{")
	if jstart >= 0:
		hsh = json.loads(line[jstart:])
		if hsh["origin"] == "omelasticsearch":
			actual = hsh
if not expected == actual:
	sys.stderr.write("ERROR: expected stats not equal to actual stats\n")
	sys.stderr.write("ERROR: expected {}\n".format(expected))
	sys.stderr.write("ERROR: actual {}\n".format(actual))
	sys.exit(1)
' $NUMMESSAGES $REBIND_INTERVAL || { rc=$?; echo error: expected stats not found in ${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log; }
else
	echo error: stats file ${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log not found
	rc=1
fi

if [ $rc != 0 ] ; then
	error_exit 1
else
	exit_test
fi
