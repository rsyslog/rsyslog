#!/bin/bash
# Verify that explicit omelasticsearch searchType use emits a deprecation warning.
. ${srcdir:=.}/diag.sh init
require_plugin omelasticsearch

WARN="omelasticsearch: parameter 'searchType' is deprecated because Elasticsearch mapping types are obsolete; remove this parameter for Elasticsearch 8 and later"

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omelasticsearch"
       server="localhost"
       serverport="19200"
       searchIndex="rsyslog_testbench"
       searchType="_doc"
       template="outfmt")
'

rsyslogd_config_check > "${RSYSLOG_DYNNAME}.with-searchtype.log" 2>&1
if [ $? -ne 0 ]; then
        echo "FAIL: config with searchType did not validate"
        cat "${RSYSLOG_DYNNAME}.with-searchtype.log"
        error_exit 1
fi
content_check "$WARN" "${RSYSLOG_DYNNAME}.with-searchtype.log"

generate_conf
add_conf '
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omelasticsearch"
       server="localhost"
       serverport="19200"
       searchIndex="rsyslog_testbench"
       template="outfmt")
'

rsyslogd_config_check > "${RSYSLOG_DYNNAME}.without-searchtype.log" 2>&1
if [ $? -ne 0 ]; then
        echo "FAIL: config without searchType did not validate"
        cat "${RSYSLOG_DYNNAME}.without-searchtype.log"
        error_exit 1
fi
if grep -F "$WARN" "${RSYSLOG_DYNNAME}.without-searchtype.log" > /dev/null; then
        echo "FAIL: config without searchType emitted deprecation warning"
        cat "${RSYSLOG_DYNNAME}.without-searchtype.log"
        error_exit 1
fi

exit_test
