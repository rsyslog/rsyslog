#!/bin/bash
# Copyright 2015-01-29 by Tim Eifler
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[abort-uncleancfg-goodcfg.sh\]: testing abort on unclean configuration
echo "testing a good Configuration verification run"
. $srcdir/diag.sh init
startup abort-uncleancfg-goodcfg.conf 
. $srcdir/diag.sh tcpflood -m10 -i1 
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

if [ ! -e rsyslog.out.log ]
then
        echo "error: expected file does not exist"
	error_exit 1
fi
exit_test
