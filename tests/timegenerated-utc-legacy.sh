#!/bin/bash
# added 2016-03-22 by RGerhards, released under ASL 2.0
#
# NOTE: faketime does NOT properly support subseconds,
# so we must ensure we do not use them. Actually, what we
# see is uninitialized data value in tv_usec, which goes
# away as soon as we do not run under faketime control.
# FOR THE SAME REASON, there is NO VALGRIND EQUIVALENT
# of this test, as valgrind would abort with reports
# of faketime.
#
# IMPORTANT: we use legacy style for the template to ensure
# that subseconds works properly for this as well. This is
# a frequent use case.
#
. ${srcdir:=.}/diag.sh init
. $srcdir/faketime_common.sh
export TZ=TEST+02:00
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%timegenerated:::date-utc%\n"
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'

echo "***SUBTEST: check 2016-03-01"
FAKETIME='2016-03-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="Mar  1 14:00:00"
cmp_exact
exit_test
