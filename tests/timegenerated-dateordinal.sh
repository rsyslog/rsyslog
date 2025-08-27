#!/bin/bash
# test many concurrent tcp connections
# add 2016-03-02 by RGerhards, released under ASL 2.0
# Note: we run several subtests here in order to save us
# from creating additional tests
# requires faketime
echo \[timegenerated-dateordinal\]: check valid dates with ordinal format
. ${srcdir:=.}/diag.sh init

. $srcdir/faketime_common.sh

export TZ=UTC+00:00

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%timegenerated:::date-ordinal%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'


echo "***SUBTEST: check 1970-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='1970-01-01 00:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact


echo "***SUBTEST: check 2000-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2000-03-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="061"
cmp_exact


echo "***SUBTEST: check 2016-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-01-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact


echo "***SUBTEST: check 2016-02-29"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-02-29 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="060"
cmp_exact


echo "***SUBTEST: check 2016-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-03-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="061"
cmp_exact


echo "***SUBTEST: check 2016-03-03"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-03-03 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="063"
cmp_exact


echo "***SUBTEST: check 2016-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-12-31 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="366"
cmp_exact


echo "***SUBTEST: check 2017-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2017-01-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact


echo "***SUBTEST: check 2020-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2020-03-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="061"
cmp_exact


echo "***SUBTEST: check 2038-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2038-01-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact


rsyslog_testbench_require_y2k38_support


echo "***SUBTEST: check 2038-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2038-12-31 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="365"
cmp_exact


echo "***SUBTEST: check 2040-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2040-01-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact


echo "***SUBTEST: check 2040-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2040-12-31 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="366"
cmp_exact


echo "***SUBTEST: check 2100-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2100-01-01 12:00:00' startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="001"
cmp_exact

exit_test
