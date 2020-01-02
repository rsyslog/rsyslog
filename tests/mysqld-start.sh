#!/bin/bash
# This is not a real test, but a script to start mysql. It is
# implemented as test so that we can start mysql at the time we need
# it (do so via Makefile.am).
# Copyright (C) 2018 Rainer Gerhards and Adiscon GmbH
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
echo pre-start
ps -ef |grep bin.mysqld
if [ "$MYSQLD_START_CMD" == "" ]; then
	exit_test # no start needed
fi

test_error_exit_handler() {
	set -x; set -v
	printf 'mysqld startup failed, log is:\n'
	$SUDO cat /var/log/mysql/error.log
}

printf 'starting mysqld...\n'
$MYSQLD_START_CMD &
wait_startup_pid /var/run/mysqld/mysqld.pid
$SUDO tail -n30 /var/log/mysql/error.log
printf 'preparing mysqld for testbench use...\n'
$SUDO ${srcdir}/../devtools/prep-mysql-db.sh
printf 'done, mysql ready for testbench\n'
ps -ef |grep bin.mysqld
exit_test
