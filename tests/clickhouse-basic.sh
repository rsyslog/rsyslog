#!/bin/bash
# add 2018-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. ${srcdir}/clickhouse-mock.sh

clickhouse_mock_setup
export NUMMESSAGES=1

generate_conf
add_conf "
module(load=\"../plugins/omclickhouse/.libs/omclickhouse\")
"
add_conf "
template(name=\"outfmt\" option.stdsql=\"on\" type=\"string\" string=\"INSERT INTO rsyslog.basic (id, severity, facility, timestamp, ipaddress, tag, message) VALUES (%msg:F,58:2%, %syslogseverity%, %syslogfacility%, '%timereported:::date-unixtimestamp%', '%fromhost-ip%', '%syslogtag%', '%msg%')\")
"
add_conf "
:syslogtag, contains, \"tag\" action(type=\"omclickhouse\" server=\"127.0.0.1\" port=\"${CLICKHOUSE_MOCK_PORT}\" bulkmode=\"off\" usehttps=\"off\"
                                        user=\"default\" pwd=\"\" template=\"outfmt\")
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

export EXPECTED="INSERT INTO rsyslog.basic (id, severity, facility, timestamp, ipaddress, tag, message) VALUES (0, 7, 20, '%timereported:::date-unixtimestamp%', '127.0.0.1', 'tag', 'msgnum:00000000:')"
# Replace runtime timestamp placeholder with the actual epoch seen in the request before comparing.
ACTUAL=$(cat "$REQ_FILE")
STAMP=$(printf '%s\n' "$ACTUAL" | sed -n "s/.*VALUES (0, 7, 20, '\([0-9]\+\)',.*/\1/p")
if [ -z "$STAMP" ]; then
    echo "omclickhouse request did not include a timestamp"
    error_exit 1
fi
EXPECTED_RENDERED=${EXPECTED//%timereported:::date-unixtimestamp%/$STAMP}
export EXPECTED="$EXPECTED_RENDERED"
cmp_exact "$REQ_FILE"

clickhouse_mock_teardown
exit_test
