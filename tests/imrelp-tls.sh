#!/bin/bash
# addd 2019-01-31 by PascalWithopf, released under ASL 2.0
# Validate RELP/TLS certvalid input with a direct tcpflood call because this
# test intentionally accepts rc=1 for older RELP/TLS stacks. The direct call
# still uses a proper-termination marker on rc=0 and treats abort statuses as
# hard tcpflood failures.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
do_skip=0
tcpflood_marker="${RSYSLOG_DYNNAME}.tcpflood.direct.proper-termination"
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'" tls="on"
		tls.cacert="'$srcdir'/tls-certs/ca.pem"
		tls.mycert="'$srcdir'/tls-certs/cert.pem"
		tls.myprivkey="'$srcdir'/tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
# Use the tcpflood binary directly here because this test intentionally accepts
# tcpflood rc=1 when older RELP/TLS stacks cannot set certvalid mode.
rm -f "$tcpflood_marker"
./tcpflood -q "$tcpflood_marker" -Trelp-tls -acertvalid -p"$TCPFLOOD_PORT" -m"$NUMMESSAGES" -x "$srcdir/tls-certs/ca.pem" -z "$srcdir/tls-certs/key.pem" -Z "$srcdir/tls-certs/cert.pem" -Ersyslog 2> "$RSYSLOG_DYNNAME.tcpflood"
tcpflood_rc=$?
if [ "$tcpflood_rc" -eq 0 ]; then
	if ! tcpflood_marker_is_valid "$tcpflood_marker"; then
		echo "FAIL: tcpflood marker missing or invalid"
		print_tcpflood_marker_file "$tcpflood_marker"
		error_exit 1
	fi
elif [ "$tcpflood_rc" -gt 128 ]; then
	echo "FAIL: tcpflood aborted with status $tcpflood_rc"
	print_tcpflood_marker_file "$tcpflood_marker"
	error_exit 1
elif [ "$tcpflood_rc" -eq 1 ]; then
	cat "$RSYSLOG_DYNNAME.tcpflood"
	if ! grep "could net set.*certvalid" < "$RSYSLOG_DYNNAME.tcpflood" ; then
		printf "librelp too old, need to skip this test\n"
		do_skip=1
	fi
else
	echo "FAIL: tcpflood exited with unexpected status $tcpflood_rc"
	print_tcpflood_marker_file "$tcpflood_marker"
	error_exit 1
fi
cat -n "$RSYSLOG_DYNNAME.tcpflood"
shutdown_when_empty
wait_shutdown
if [ $do_skip -eq 1 ]; then
	skip_test
fi
seq_check
exit_test
