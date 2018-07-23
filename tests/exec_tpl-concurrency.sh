#!/bin/bash
# Test concurrency of exec_template function with msg variables
# Added 2015-12-11 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[exec_tpl-concurrency.sh\]: testing concurrency of exec_template w variables

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
startup exec_tpl-concurrency.conf
sleep 1
. $srcdir/diag.sh tcpflood -m500000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 499999
exit_test
