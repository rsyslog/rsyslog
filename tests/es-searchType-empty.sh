#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
# Using the default will cause deprecation failures
export ES_PORT_OPTION="transport.port"
export NUMMESSAGES=2000 # slow test
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
ensure_elasticsearch_ready
generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="19200"
	       searchType=""
	       template="tpl"
	       searchIndex="rsyslog_testbench")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
es_getdata
seq_check
if grep "DEPRECATION" $dep_work_dir/es/logs/rsyslog-testbench_deprecation.log; then
	echo "Found deprecations, failing!"
	exit 1
fi
exit_test
