#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# With octet-counted framing disabled, send the no_octet_counted fixture
# through imptcp. The output must contain sequence 0 through 19 after
# shutdown.
echo ====================================================================================
echo TEST: \[imptcp_no_octet_counted.sh\]: test imptcp with octet counted framing disabled
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="remote" supportOctetCountedFraming="off")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -B -I ${srcdir}/testsuites/no_octet_counted.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19
exit_test
