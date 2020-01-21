#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
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
	       template="tpl"
	       writeoperation="create"
	       searchIndex="rsyslog_testbench")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'

startup
injectmsg  0 1
shutdown_when_empty
wait_shutdown
if grep -q "omelasticsearch: writeoperation '1' requires bulkid"  $RSYSLOG_OUT_LOG ; then
	echo found correct error message
else
	echo Error: did not complain about incorrect writeoperation
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="19200"
	       template="tpl"
	       writeoperation="unknown"
	       searchIndex="rsyslog_testbench")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'

startup
injectmsg  0 1
shutdown_when_empty
wait_shutdown
if grep -q "omelasticsearch: invalid value 'unknown' for writeoperation"  $RSYSLOG_OUT_LOG ; then
	echo found correct error message
else
	echo Error: did not complain about incorrect writeoperation
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="id-template" type="list") { constant(value="123456789") }

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="19200"
	       template="tpl"
	       writeoperation="create"
	       bulkid="id-template"
	       dynbulkid="on"
	       bulkmode="on"
	       searchIndex="rsyslog_testbench")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'

init_elasticsearch

export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
export NUMMESSAGES=1
startup
injectmsg
shutdown_when_empty
wait_shutdown
es_getdata 1 $ES_PORT

$PYTHON <$RSYSLOG_DYNNAME.work -c '
import sys,json
hsh = json.load(sys.stdin)
try:
	if hsh["hits"]["hits"][0]["_id"] == "123456789":
		print "good - found expected value"
		sys.exit(0)
	print "Error: _id not expected value 123456789:", hsh["hits"]["hits"][0]["_id"]
	sys.exit(1)
except ValueError:
	print "Error: output is not valid:", json.dumps(hsh,indent=2)
	sys.exit(1)
'

if [ $? -eq 0 ] ; then
	echo found correct response
else
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi
exit_test
