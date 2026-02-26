#!/bin/bash
# Test named rate limits for omelasticsearch (config-validation)
# Verifies that omelasticsearch accepts ratelimit.name without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
ratelimit(name="omelasticsearch_test_limit" interval="2" burst="5")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omelasticsearch" server="localhost" serverport="19200"
       ratelimit.name="omelasticsearch_test_limit"
       retryfailures="on" template="outfmt")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
