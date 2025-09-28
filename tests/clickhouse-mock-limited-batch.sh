#!/bin/bash
## @file clickhouse-mock-limited-batch.sh
## @brief Exercise omclickhouse maxbytes batching against the mock server.
. ${srcdir:=.}/diag.sh init
. ${srcdir}/clickhouse-mock.sh

MODULE_PATH="${srcdir}/../plugins/omclickhouse/.libs/omclickhouse"
if [ ! -f "$MODULE_PATH" ]; then
    MODULE_PATH="../plugins/omclickhouse/.libs/omclickhouse"
fi
if [ ! -f "$MODULE_PATH" ]; then
    script_name=$(basename "$0")
    echo "$script_name: omclickhouse module is not available"
    exit 77
fi

clickhouse_mock_setup
export NUMMESSAGES=6

generate_conf
add_conf "
module(load=\"$MODULE_PATH\")
"
add_conf "
template(name=\"outfmt\" option.stdsql=\"on\" type=\"string\" string=\"INSERT INTO rsyslog.limited (id, ipaddress, message) VALUES (%msg:F,58:2%, '%fromhost-ip%', '%msg:F,58:2%')\")
"
add_conf "
:syslogtag, contains, \"tag\" action(type=\"omclickhouse\" server=\"127.0.0.1\" port=\"${CLICKHOUSE_MOCK_PORT}\"
                                        user=\"default\" pwd=\"\" template=\"outfmt\" maxbytes=\"64\" usehttps=\"off\")
"

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

REQ_COUNT=$(clickhouse_mock_request_count)
if [ "$REQ_COUNT" -lt 2 ]; then
    echo "expected batching to split into multiple requests, saw $REQ_COUNT"
    error_exit 1
fi

FIRST_REQ=$(clickhouse_mock_request_file 1)
LAST_REQ=$(clickhouse_mock_request_file "$REQ_COUNT")

content_check "INSERT INTO rsyslog.limited" "$FIRST_REQ"
content_check "VALUES (0, '127.0.0.1'" "$FIRST_REQ"
content_check "msgnum:00000005:" "$LAST_REQ"

clickhouse_mock_teardown
exit_test
