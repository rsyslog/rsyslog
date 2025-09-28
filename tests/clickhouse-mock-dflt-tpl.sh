#!/bin/bash
## @file clickhouse-mock-dflt-tpl.sh
## @brief Validate default omclickhouse template output with the mock server.
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
export NUMMESSAGES=1

generate_conf
add_conf "
module(load=\"$MODULE_PATH\")
"
add_conf "
:syslogtag, contains, \"tag\" action(type=\"omclickhouse\" server=\"127.0.0.1\" port=\"${CLICKHOUSE_MOCK_PORT}\" bulkmode=\"off\"
                                        usehttps=\"off\" user=\"default\" pwd=\"\")
"

startup
injectmsg
shutdown_when_empty
wait_shutdown

REQ_FILE=$(clickhouse_mock_request_file 1)
if [ ! -f "$REQ_FILE" ]; then
    echo "omclickhouse mock did not record a request"
    error_exit 1
fi

ACTUAL=$(cat "$REQ_FILE")
if ! printf '%s\n' "$ACTUAL" | grep -q "INSERT INTO rsyslog.SystemEvents"; then
    echo "unexpected SQL payload: missing INSERT"
    cat -n "$REQ_FILE"
    error_exit 1
fi

if ! printf '%s\n' "$ACTUAL" | grep -q "VALUES (7, 20,"; then
    echo "unexpected SQL payload: severity/facility mismatch"
    cat -n "$REQ_FILE"
    error_exit 1
fi

if ! printf '%s\n' "$ACTUAL" | grep -q "'tag', 'msgnum:00000000:'"; then
    echo "unexpected SQL payload: message template mismatch"
    cat -n "$REQ_FILE"
    error_exit 1
fi

STAMP=$(printf '%s\n' "$ACTUAL" | sed -n "s/.*VALUES (7, 20, '\([0-9]\+\)',.*/\1/p")
if [ -z "$STAMP" ]; then
    echo "omclickhouse request did not include a timestamp"
    error_exit 1
fi

clickhouse_mock_teardown
exit_test
