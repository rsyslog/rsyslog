#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
ensure_elasticsearch_ready
generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")


ruleset(name="stats") {
  action(type="omfile" file="'${RSYSLOG_DYNNAME}'.out.stats.log")
}
module(load="../plugins/impstats/.libs/impstats" interval="2" severity="7" resetCounters="off" Ruleset="stats" bracketing="on" format="json")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
                                 server=["localhost", "http://localhost/", "localhost:9201"]
                                 serverport="19200"
                                 template="tpl"
                                 searchType="_doc"
                                 searchIndex="rsyslog_testbench")
'
startup
injectmsg  0 100
wait_queueempty
wait_for_stats_flush ${RSYSLOG_DYNNAME}.out.stats.log
shutdown_when_empty
wait_shutdown 
es_getdata 100 19200
seq_check  0 99
# The configuration makes every other request from message #3 fail checkConn (N/2-1)
custom_content_check '"failed.checkConn": 49' "${RSYSLOG_DYNNAME}.out.stats.log"
exit_test
