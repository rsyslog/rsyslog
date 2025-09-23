#!/bin/bash
# The sole purpose of this test is to check that rsyslog "survives" the
# duplicate ruleset definition and emits the proper error message. So we
# do NOT need to have elasticsearch running to carry it out. We avoid
# sending data to ES as this complicates things more than needed.
# This is based on a real case, see
#     https://github.com/rsyslog/rsyslog/pull/3796
# Thanks to Noriko Hosoi for providing the original test idea which
# was then modified by Rainer Gerhards.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
generate_conf
add_conf '
template(name="tpl" type="string" string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
ruleset(name="try_es") {
                action(type="omelasticsearch"
                                 server="localhost"
                                 serverport=`echo $ES_PORT`
                                 template="tpl"
                                 searchIndex="rsyslog_testbench"
                                 retryruleset="try_es"
                                 )
}

ruleset(name="try_es") {
                action(type="omelasticsearch"
                                 server="localhost"
                                 serverport=`echo $ES_PORT`
                                 template="tpl"
                                 searchIndex="rsyslog_testbench"
                                 retryruleset="try_es"
                                 )
}
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_immediate
wait_shutdown 
content_check "ruleset 'try_es' specified more than once"
exit_test
