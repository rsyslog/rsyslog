#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
ensure_elasticsearch_ready

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then {
        action(type="omelasticsearch"
                server="127.0.0.1"
                serverport="19200"
                template="tpl"
                searchType="_doc"
                action.resumeRetryCount="-1"
                action.resumeInterval="1"
                searchIndex="rsyslog_testbench")

	# this action just to count processed messages
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.syncfile")
}
'
startup
injectmsg 0 $NUMMESSAGES
wait_file_lines $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES
echo stop elasticsearch...
stop_elasticsearch
injectmsg $NUMMESSAGES 1000
NUMMESSAGES=$(( NUMMESSAGES + 1000 ))
echo start elasticsearch...
start_elasticsearch
wait_file_lines $RSYSLOG_DYNNAME.syncfile $NUMMESSAGES
shutdown_when_empty
wait_shutdown
./msleep 1000 # ES might need some time to maintain index...
es_getdata $(( NUMMESSAGES + 2000 )) 19200
#stop_elasticsearch

seq_check  0 $((NUMMESSAGES - 1)) -d
cleanup_elasticsearch
exit_test
