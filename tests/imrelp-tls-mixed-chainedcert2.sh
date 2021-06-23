#!/bin/bash
# addd 2020-08-25 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_relpEngineVersion "1.7.0"
export NUMMESSAGES=1000

# uncomment for debugging support:
export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
export TCPFLOOD_EXTRA_OPTS="-v" 

# Print Versions
echo "OpenSSL Version:"
openssl version

echo "GnuTls Version:"
gnutls-cli --version

do_skip=0
generate_conf
add_conf '
# uncomment for debugging support:
# $DebugFile debug.log
# $DebugLevel 2

module(	load="../plugins/imrelp/.libs/imrelp"
	tls.tlslib="gnutls"
)
input(type="imrelp" port="'$TCPFLOOD_PORT'" 
		tls="on"
		tls.mycert="'$srcdir'/tls-certs/certchained.pem"
		tls.myprivkey="'$srcdir'/tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'

startup
./tcpflood -u openssl -Trelp-tls -acertvalid -p$TCPFLOOD_PORT -m$NUMMESSAGES -z "$srcdir/tls-certs/key.pem" -Z "$srcdir/tls-certs/certchained.pem" -Ersyslog 2> $RSYSLOG_DYNNAME.tcpflood
cat -n $RSYSLOG_DYNNAME.tcpflood
shutdown_when_empty
wait_shutdown

# uncomment for debugging support:
# cat debug.log

if [ $do_skip -eq 1 ]; then
	skip_test
fi
seq_check
exit_test
