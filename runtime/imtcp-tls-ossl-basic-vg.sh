#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released  under GPLv3
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
#	debug.whitelist="on"
#	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )
input(	type="imtcp"
	port="13514" )

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(	type="omfile" 
					template="outfmt"
					file="rsyslog.out.log")
'
# Begin actuall testcase
. $srcdir/diag.sh startup-vg-noleak
#./msleep 2000
. $srcdir/diag.sh tcpflood -p13514 -m10000 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit