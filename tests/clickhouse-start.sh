#!/bin/bash
# This is not a real test, but a script to start clickhouse. It is
# implemented as test so that we can start clickhouse at the time we need
# it (do so via Makefile.am).
# Copyright (C) 2018 Pascal Withopf and Adiscon GmbH
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
set -x
if [ "$CLICKHOUSE_START_CMD" == "" ]; then
	exit_test # no start needed
fi

test_error_exit_handler() {
	printf 'clickhouse startup failed, log is:\n'
	$SUDO cat /var/log/clickhouse-server/clickhouse-server.err.log
}

printf 'starting clickhouse...\n'
$CLICKHOUSE_START_CMD &
sleep 10
#wait_startup_pid /var/run/clickhouse-server/clickhouse-server.pid
printf 'preparing clickhouse for testbench use...\n'
$SUDO ${srcdir}/../devtools/prepare_clickhouse.sh
clickhouse-client --query="select 1"
rc=$?
if [ $rc -ne 0 ]; then
	printf 'clickhouse failed to start, exit code %d\n' $rc
	error_exit 100
fi
printf 'done, clickhouse ready for testbench\n'
exit_test
