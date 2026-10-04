#!/bin/bash
# Verify that the default ompgsql schema accepts a Docker-style syslog tag
# longer than the historical 60-character column. The exact value selected
# from PostgreSQL after synchronized shutdown is the regression oracle.

. ${srcdir:=.}/diag.sh init

psql -h localhost -U postgres -f ${srcdir}/testsuites/pgsql-basic.sql

generate_conf
add_conf '
module(load="../plugins/ompgsql/.libs/ompgsql")
:msg, contains, "ompgsql-i7559-long-tag" :ompgsql:127.0.0.1,syslogtest,postgres,testbench
'
startup

LONG_TAG="docker-compose_project_long-service-name_with-very-descriptive-instance-identifier"
injectmsg_literal "<13>Mar 10 01:00:00 docker-host ${LONG_TAG}: ompgsql-i7559-long-tag"
shutdown_when_empty
wait_shutdown

psql -h localhost -U postgres -d syslogtest -c \
	'SELECT SysLogTag FROM SystemEvents;' -t -A > "$RSYSLOG_OUT_LOG"

export EXPECTED="${LONG_TAG}:"
cmp_exact

echo cleaning up test database
psql -h localhost -U postgres -c 'DROP DATABASE IF EXISTS syslogtest;'

exit_test
