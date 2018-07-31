#!/bin/bash
# test many concurrent tcp connections
# addd 2016-03-02 by RGerhards, released under ASL 2.0
# Note: we run several subtests here in order to save us
# from creating additional tests
# requires faketime
echo \[timegenerated-dateordinal\]: check valid dates with ordinal format
. $srcdir/diag.sh init

. $srcdir/faketime_common.sh

export TZ=UTC+00:00

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun 13514

template(name="outfmt" type="string"
	 string="%timegenerated:::date-ordinal%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'


echo "***SUBTEST: check 1970-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='1970-01-01 00:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2000-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2000-03-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "061" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2016-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-01-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2016-02-29"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-02-29 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "060" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2016-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-03-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "061" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2016-03-03"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-03-03 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "063" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2016-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2016-12-31 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "366" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2017-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2017-01-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2020-03-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2020-03-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "061" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2038-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2038-01-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


rsyslog_testbench_require_y2k38_support


echo "***SUBTEST: check 2038-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2038-12-31 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "365" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2040-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2040-01-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2040-12-31"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2040-12-31 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "366" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


echo "***SUBTEST: check 2100-01-01"
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
FAKETIME='2100-01-01 12:00:00' startup
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "001" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid timestamps generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;


exit_test
