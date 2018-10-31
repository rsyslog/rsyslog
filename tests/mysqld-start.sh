#!/bin/bash
# This is not a real test, but a script to start mysql. It is
# implemented as test so that we can start mysql at the time we need
# it (do so via Makefile.am).
# Copyright (C) 2018 Rainer Gerhards and Adiscon GmbH
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
if [ "$MYSQLD_START_CMD" == "" ]; then
	exit_test # no start needed
fi
printf 'starting mysqld...\n'
$MYSQLD_START_CMD &
wait_startup_pid /var/run/mysqld/mysqld.pid
printf 'preparing mysqld for testbench use...\n'
$SUDO ${srcdir}/../devtools/prep-mysql-db.sh
printf 'done, mysql ready for testbench\n'
exit_test
