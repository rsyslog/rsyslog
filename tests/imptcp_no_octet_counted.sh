#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imptcp_no_octet_counted.sh\]: test imptcp with octet counted framing disabled
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="remote" supportOctetCountedFraming="off")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
startup
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/no_octet_counted.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 19
exit_test
