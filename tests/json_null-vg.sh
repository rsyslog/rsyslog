#!/bin/bash
# added 2015-11-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
# Note: the aim of this test is to test against misadressing, so we do
# not actually check the output

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[json_null.sh\]: test for json containung \"null\" value
. $srcdir/diag.sh init
startup_vg json_null.conf
. $srcdir/diag.sh tcpflood -m 1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: @cee: { \\\"nope\\\": null }\""
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
exit_test
