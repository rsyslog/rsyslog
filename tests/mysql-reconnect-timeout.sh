#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Regression coverage for issue #4781: after an idle MySQL wait_timeout, error
# 4031 on START TRANSACTION must reconnect before message SQL begins, preserving
# the following message. Retrying a later statement could lose earlier writes
# from its transaction, so this test specifically exercises transaction start.
# Only injected msgnum messages reach ommysql, so startup diagnostics cannot
# affect the exact row-count and sequential-row oracles. The oracle is exactly
# one row after each separately injected message and two sequential rows at
# shutdown. Setting wait_timeout to 2 seconds and waiting 4 seconds (twice the
# timeout) after the first committed row gives the server and loaded CI runner
# margin to reap the idle session, while avoiding expiry during the first write.
# On irrelevant PR changes, skip before querying MySQL because mysqld-start.sh
# also skips service startup for that family.
. ${srcdir:=.}/diag.sh init
ensure_module_needs_testing mysql
require_plugin ommysql
# Match mysqld-start before the version probe accesses the shared service.
ensure_module_needs_testing mysql

mysql_test_error() {
	printf 'TESTBENCH_ERROR: %s\n' "$1" >&2
	error_exit 1
}

MYSQL_SERVER_VERSION="$($SUDO mysql --user=root --batch --skip-column-names --execute='SELECT VERSION()')" \
    || mysql_test_error "could not query MySQL server version"
if printf '%s\n' "$MYSQL_SERVER_VERSION" | grep -qi 'mariadb'; then
	printf 'SKIP: MariaDB uses error 4031 for a different SQL error\n'
	exit 77
fi

MYSQL_VERSION_NUMBER="${MYSQL_SERVER_VERSION%%-*}"
IFS=. read -r mysql_major mysql_minor mysql_patch <<< "$MYSQL_VERSION_NUMBER"
case "$mysql_major:$mysql_minor:$mysql_patch" in
	*[!0-9:]*|::*|*::*) mysql_test_error "unexpected MySQL server version: $MYSQL_SERVER_VERSION" ;;
esac
mysql_version_id=$((mysql_major * 10000 + mysql_minor * 100 + mysql_patch))
if [ "$mysql_version_id" -lt 80024 ]; then
	printf 'SKIP: MySQL 8.0.24 or later is needed for error 4031 (server is %s)\n' "$MYSQL_SERVER_VERSION"
	exit 77
fi

mysql_prep_for_test

old_wait_timeout="$($SUDO mysql --user=root --batch --skip-column-names \
	--execute='SELECT @@GLOBAL.wait_timeout')" || mysql_test_error "could not read MySQL wait_timeout"
case "$old_wait_timeout" in
	''|*[!0-9]*) mysql_test_error "unexpected MySQL wait_timeout: $old_wait_timeout" ;;
esac

restore_wait_timeout() {
	if [ -n "$old_wait_timeout" ]; then
		$SUDO mysql --user=root --execute="SET GLOBAL wait_timeout=$old_wait_timeout" >/dev/null || return 1
		old_wait_timeout=
	fi
}
trap restore_wait_timeout EXIT
$SUDO mysql --user=root --execute='SET GLOBAL wait_timeout=2' || mysql_test_error "could not set MySQL wait_timeout"

generate_conf
add_conf '
module(load="../plugins/ommysql/.libs/ommysql")

template(name="tpl" type="string" string="insert into SystemEvents (Message) values (\"%msg%\")" option.sql="on")

if $msg contains "msgnum:" then {
	action(type="ommysql"
		server="127.0.0.1"
		db="'$RSYSLOG_DYNNAME'"
		uid="rsyslog"
		pwd="testbench"
		template="tpl"
		queue.type="Direct")
}
'

mysql_row_count() {
	mysql --batch --skip-column-names --user=rsyslog --password=testbench \
		--database="$RSYSLOG_DYNNAME" --execute='SELECT COUNT(*) FROM SystemEvents' 2>/dev/null || echo 0
}

wait_for_mysql_rows() {
	wait_file_lines --abort-on-oversize --count-function mysql_row_count \
		"$RSYSLOG_OUT_LOG" "$1" "$TB_TEST_TIMEOUT"
}

startup
injectmsg 0 1
wait_for_mysql_rows 1
sleep 4
injectmsg 1 1
wait_for_mysql_rows 2
shutdown_when_empty
wait_shutdown

restore_wait_timeout || mysql_test_error "could not restore MySQL wait_timeout"
trap - EXIT
mysql_get_data
seq_check 0 1
mysql_cleanup_test
exit_test
