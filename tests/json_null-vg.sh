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
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

# we must make sure the template contains a reference to the 
# data item with null value
template(name="outfmt" type="string" string="%$!nope%\n")
template(name="outfmt-all-json" type="string" string="%$!all-json%\n")

action(type="mmjsonparse")
action(type="omfile" file="./rsyslog.out.log" template="outfmt")
if $!nope == "" then
	action(type="omfile" file="./rsyslog2.out.log" template="outfmt-all-json")
'
startup_vg
. $srcdir/diag.sh tcpflood -m 1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: @cee: { \\\"nope\\\": null }\""
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
exit_test
