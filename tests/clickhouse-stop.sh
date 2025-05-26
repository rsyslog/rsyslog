#!/bin/bash
# This is not a real test, but a script to stop clickhouse. It is
# implemented as test so that we can stop clickhouse at the time we need
# it (do so via Makefile.am).
# Copyright (C) 2018 Pascal Withopf and Adiscon GmbH
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
if [ "$CLICKHOUSE_STOP_CMD" == "" ]; then
	exit_test
fi

clickhouse-client --query="DROP DATABASE rsyslog"
sleep 1
printf 'stopping clickhouse...\n'
#$SUDO sed -n -r 's/PID: ([0-9]+\.*)/\1/p' /var/lib/clickhouse/status > /tmp/clickhouse-server.pid
#$SUDO kill $($SUDO sed -n -r 's/PID: ([0-9]+\.*)/\1/p' /var/lib/clickhouse/status)
eval $CLICKHOUSE_STOP_CMD
sleep 1 # cosmetic: give clickhouse a chance to emit shutdown message
exit_test
