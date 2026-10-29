#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Check module-wide counters against two successful actions and one failed SQL
# transaction, with reporting blocked until queues drain. Consecutive controlled
# snapshots also prove resetCounters resets all four counters.
. ${srcdir:=.}/diag.sh init
psql -h localhost -U postgres -v ON_ERROR_STOP=1 -f "${srcdir}/testsuites/pgsql-basic.sql" || error_exit 1
generate_conf
add_conf '
main_queue(queue.workerThreads="2" queue.workerThreadMinimumMessages="1"
           queue.dequeueBatchSize="16")
module(load="../plugins/ompgsql/.libs/ompgsql")
module(load="../plugins/impstats/.libs/impstats" interval="1" resetCounters="on"
       log.syslog="off" log.file="'"$RSYSLOG_DYNNAME"'.stats" format="json")
template(name="badSql" type="string" option.sql="on"
         string="INSERT INTO ompgsql_stats_missing_table VALUES (1)")
if $msg contains "msgnum" then {
    action(type="ompgsql" server="127.0.0.1" db="syslogtest"
           user="postgres" pass="testbench")
    action(type="ompgsql" server="127.0.0.1" db="syslogtest"
           user="postgres" pass="testbench")
}
if $msg contains "pgsql-stats-invalid" then {
    action(type="ompgsql" server="127.0.0.1" db="syslogtest"
           user="postgres" pass="testbench" template="badSql"
           action.resumeRetryCount="0")
}'
startup
. "$srcdir/diag.sh" block-stats-flush
injectmsg 0 64
wait_queueempty
. "$srcdir/diag.sh" allow-single-stats-flush-after-block-and-wait-for-it
. "$srcdir/diag.sh" allow-single-stats-flush-after-block-and-wait-for-it
injectmsg_literal '<13>Mar 10 01:00:00 host app: pgsql-stats-invalid'
wait_queueempty
. "$srcdir/diag.sh" allow-single-stats-flush-after-block-and-wait-for-it
. "$srcdir/diag.sh" await-stats-flush-after-block
shutdown_when_empty
wait_shutdown
count=$(psql -h localhost -U postgres -d syslogtest -t -A -c 'SELECT count(*) FROM SystemEvents')
test "$count" = 128 || error_exit 1
python3 - "$RSYSLOG_DYNNAME.stats" <<'PY'
import json
import sys

with open(sys.argv[1]) as source:
    rows = [json.loads(line[line.index('{'):]) for line in source if line.strip()]
rows = [row for row in rows if row.get('origin') == 'ompgsql']
assert rows, 'ompgsql statistics are absent'
keys = ('submitted', 'committed', 'transactions', 'failed')
assert all(all(key in row for key in keys) for row in rows), rows
success = next(i for i, row in enumerate(rows)
               if row['submitted'] == 128 and row['committed'] == 128
               and 2 <= row['transactions'] <= 128 and row['failed'] == 0)
assert any(all(row[key] == 0 for key in keys) for row in rows[success + 1:]), rows
assert any(row['submitted'] >= 1 and row['committed'] == 0
           and row['transactions'] == 0 and row['failed'] == row['submitted']
           for row in rows[success + 1:]), rows
PY
test $? -eq 0 || error_exit 1
psql -h localhost -U postgres -c 'DROP DATABASE IF EXISTS syslogtest;'
exit_test
