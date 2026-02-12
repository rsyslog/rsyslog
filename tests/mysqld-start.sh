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

ensure_mysqld_datadir() {
	# Recent distro images may ship mysql-server without an initialized datadir.
	if $SUDO test -d /var/lib/mysql/mysql; then
		return 0
	fi

	printf 'mysql datadir is not initialized, initializing now...\n'
	$SUDO mkdir -p /var/lib/mysql /var/run/mysqld
	$SUDO chown -R mysql:mysql /var/lib/mysql /var/run/mysqld

	# Some package versions leave partial files in /var/lib/mysql without system
	# tables. In that state, mysqld initialization aborts unless we start clean.
	if $SUDO find /var/lib/mysql -mindepth 1 -maxdepth 1 | grep -q .; then
		printf 'mysql datadir contains partial files, resetting it before init...\n'
		$SUDO find /var/lib/mysql -mindepth 1 -maxdepth 1 -exec rm -rf {} +
	fi
	if command -v mysqld >/dev/null 2>&1 && \
	   mysqld --help --verbose 2>/dev/null | grep -q -- '--initialize-insecure'; then
		$SUDO mysqld --initialize-insecure --user=mysql --datadir=/var/lib/mysql
	elif command -v mysql_install_db >/dev/null 2>&1; then
		$SUDO mysql_install_db --user=mysql --ldata=/var/lib/mysql
	else
		echo "error: cannot initialize mysql datadir (no supported initializer found)" >&2
		return 1
	fi
}

test_error_exit_handler() {
	set -x; set -v
	printf 'mysqld startup failed, log is:\n'
	$SUDO cat /var/log/mysql/error.log
}

printf 'starting mysqld...\n'
ensure_mysqld_datadir
$MYSQLD_START_CMD &
wait_startup_pid /var/run/mysqld/mysqld.pid
$SUDO tail -n30 /var/log/mysql/error.log
printf 'preparing mysqld for testbench use...\n'
$SUDO ${srcdir}/../devtools/prep-mysql-db.sh
printf 'done, mysql ready for testbench\n'
ps -ef |grep bin.mysqld
exit_test
