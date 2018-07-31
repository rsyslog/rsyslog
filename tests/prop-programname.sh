#!/bin/bash
# addd 2017-01142 by RGerhards, released under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%syslogtag%,%programname%\n")
local0.* action(type="omfile" template="outfmt"
	        file=`echo $RSYSLOG_OUT_LOG`)
'
startup
. $srcdir/diag.sh tcpflood -m 1 -M "\"<133>2011-03-01T11:22:12Z host tag/with/slashes msgh ...x\""
. $srcdir/diag.sh tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "tag/with/slashes,tag" | $RS_CMPCMD rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
