#!/bin/bash
# test many concurrent tcp connections
# released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test does not work on FreeBSD"
export NUMMESSAGES=40000 # we unfortunately need many messages as we have many connections
export TB_TEST_MAX_RUNTIME=1200 # this test is VERY slow, so we need to override max runtime
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$MaxOpenFiles 200
$InputTCPMaxSessions 1100
global(
	defaultNetstreamDriverCAFile="'$srcdir'/testsuites/x.509/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites'/x.509/client-cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites'/x.509/client-key.pem"
	defaultNetstreamDriver="gtls"
	debug.whitelist="on"
	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

$InputTCPServerStreamDriverMode 1
$InputTCPServerStreamDriverAuthMode anon
$InputTCPServerRun '$TCPFLOOD_PORT'

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup_vg
# the config file specifies exactly 1100 connections
tcpflood -c1000 -m$NUMMESSAGES -Ttls -x$srcdir/testsuites/x.509/ca.pem -Z$srcdir/testsuites/x.509/client-cert.pem -z$srcdir/testsuites/x.509/client-key.pem
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 2
shutdown_when_empty
wait_shutdown_vg
check_exit_vg
# we do not do a seq check, as of the design of this test some messages
# will be lost. So there is no point in checking if all were received. The
# point is that we look at the valgrind result, to make sure we do not
# have a mem leak in those error cases (we had in the past, thus the test
# to prevent that in the future).
exit_test
