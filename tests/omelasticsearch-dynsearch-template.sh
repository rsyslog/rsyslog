#!/bin/bash
# Regression test for omelasticsearch bulk mode with both a message template and
# a dynamic search-index template. The oracle is clean startup, processing, and
# shutdown with an unavailable Elasticsearch endpoint: this exercises the local
# batch sizing/build path that previously crashed before any remote service was
# needed.
. ${srcdir:=.}/diag.sh init
require_plugin omelasticsearch

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="msgTpl" type="string" string="{\"msg\":\"%msg%\"}")
template(name="idxTpl" type="string" string="rsyslog-test-%programname%")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
action(type="omelasticsearch"
       server="127.0.0.1"
       serverport="19200"
       bulkmode="on"
       dynSearchIndex="on"
       searchIndex="idxTpl"
       template="msgTpl"
       action.resumeRetryCount="0"
       action.resumeInterval="1")
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z host testprog - - - msgnum:0'
shutdown_when_empty
wait_shutdown

content_check "msgnum:0" "$RSYSLOG_OUT_LOG"
exit_test
