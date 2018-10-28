#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

options=-t
./tcpflood $options &> $RSYSLOG_OUT_LOG
content_check 'invalid option'

options=-Ttls
valgrind --error-exitcode=10 ./tcpflood $options &> $RSYSLOG_OUT_LOG
if [ $? -eq 10 ]; then
	cat "$RSYSLOG_OUT_LOG"
	printf 'FAIL: valgrind failed with options: %s\n' "$options"
	error_exit 1
fi
content_check '-z and -Z must also be specified'

exit_test
