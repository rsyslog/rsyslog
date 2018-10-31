#!/bin/bash
# This is not a real test, but a script to stop mysql. It is
# implemented as test so that we can stop mysql at the time we need
# it (do so via Makefile.am).
# Copyright (C) 2018 Rainer Gerhards and Adiscon GmbH
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
if [ "$MYSQLD_STOP_CMD" == "" ]; then
	exit_test
fi
printf 'stopping mysqld...\n'
eval $MYSQLD_STOP_CMD
sleep 1 # cosmetic: give mysqld a chance to emit shutdown message
exit_test
