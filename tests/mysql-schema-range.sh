#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the shipped ommysql schema keeps event IDs, property IDs, and links
# aligned beyond their old unsigned/signed 32-bit limits. Explicit result
# values are the oracle; any failed insert or mismatch fails the test.
. ${srcdir:=.}/diag.sh init
set -o pipefail

mysql_schema_fail() {
    printf 'FAIL: %s\n' "$1" >&2
    error_exit 1
}

test_error_exit_handler() {
    mysql --user=rsyslog --password=testbench \
        -e "DROP DATABASE IF EXISTS ${RSYSLOG_DYNNAME};" >/dev/null 2>&1 || true
}
ensure_module_needs_testing mysql

# The shipped SQL names Syslog; substitute only that database name so this
# test owns an isolated database and does not touch an existing installation.
sed -e "s/^CREATE DATABASE Syslog;$/CREATE DATABASE ${RSYSLOG_DYNNAME};/" \
    -e "s/^USE Syslog;$/USE ${RSYSLOG_DYNNAME};/" \
    "${srcdir}/../plugins/ommysql/createDB.sql" |
    mysql --user=rsyslog --password=testbench || mysql_schema_fail "could not load shipped MySQL schema"

mysql --user=rsyslog --password=testbench --database "$RSYSLOG_DYNNAME" -e '
    ALTER TABLE SystemEvents AUTO_INCREMENT=4294967295;
    INSERT INTO SystemEvents (Message) VALUES ("boundary"), ("next");
    ALTER TABLE SystemEventsProperties AUTO_INCREMENT=4294967295;
    INSERT INTO SystemEventsProperties (SystemEventID, ParamName)
        VALUES (4294967295, "boundary"), (4294967296, "next");
' || mysql_schema_fail "shipped MySQL schema rejected IDs above the old limit"

actual=$(mysql --batch --skip-column-names --user=rsyslog --password=testbench \
    --database "$RSYSLOG_DYNNAME" -e '
    SELECT e.ID, p.ID, p.SystemEventID
      FROM SystemEvents AS e
      JOIN SystemEventsProperties AS p ON p.SystemEventID = e.ID
      ORDER BY e.ID;
') || mysql_schema_fail "could not read MySQL schema range results"
expected=$(printf '4294967295\t4294967295\t4294967295\n4294967296\t4294967296\t4294967296')
[ "$actual" = "$expected" ] || mysql_schema_fail "MySQL event/property IDs or links do not match"

mysql_cleanup_test
exit_test
