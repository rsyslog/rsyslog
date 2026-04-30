#!/bin/bash
# added 2026-04-30 to guard issue #6612, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imrelp
require_plugin omrelp
require_relpEngineSetTLSLibByName

if [ ! -r /proc/$$/stat ]; then
	echo "/proc CPU accounting not available. Test stopped"
	exit 77
fi

clk_tck=$(getconf CLK_TCK 2>/dev/null)
if ! [[ "$clk_tck" =~ ^[0-9]+$ ]] || [ "$clk_tck" -le 0 ]; then
	echo "CLK_TCK unavailable. Test stopped"
	exit 77
fi

read_proc_cpu_ticks() {
	local stat fields
	stat=$(cat "/proc/$1/stat" 2>/dev/null) || return 1
	fields=${stat##*) }
	set -- $fields
	echo $((${12} + ${13}))
}

sample_cpu_delta_ticks() {
	local pid="$1"
	local interval_ms="$2"
	local start_ticks end_ticks

	start_ticks=$(read_proc_cpu_ticks "$pid") || return 1
	$TESTTOOL_DIR/msleep "$interval_ms"
	end_ticks=$(read_proc_cpu_ticks "$pid") || return 1
	echo $((end_ticks - start_ticks))
}

export NUMMESSAGES=100
export TB_TEST_MAX_RUNTIME=30
export PORT_RCVR="$(get_free_port)"

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")

input(type="imrelp"
	port="'$PORT_RCVR'"
	tls="on"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem")

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp"
	name="issue_6612_omrelp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	tls="on"
	queue.type="LinkedList")
action(type="omfile" file="'$RSYSLOG_DYNNAME.errmsgs'")
' 2
startup 2

sender_pid=$(cat "${RSYSLOG_PIDBASE}2.pid")
injectmsg2 1 $NUMMESSAGES

$TESTTOOL_DIR/msleep 500
if ! delta_ticks=$(sample_cpu_delta_ticks "$sender_pid" 1200); then
	error_exit 1 "sender process exited before CPU sampling started"
fi
threshold_ticks=$((clk_tck / 2))
if [ "$threshold_ticks" -lt 10 ]; then
	threshold_ticks=10
fi

printf 'sender pid %s consumed %d CPU ticks while handling TLS auth failure, threshold %d\n' \
	"$sender_pid" "$delta_ticks" "$threshold_ticks"

content_check "DISABLING action" "$RSYSLOG_DYNNAME.errmsgs"

shutdown_immediate 2
$TESTTOOL_DIR/msleep 100
if shutdown_delta_ticks=$(sample_cpu_delta_ticks "$sender_pid" 1200); then
	printf 'sender pid %s consumed %d CPU ticks after shutdown started, threshold %d\n' \
		"$sender_pid" "$shutdown_delta_ticks" "$threshold_ticks"
else
	shutdown_delta_ticks=0
	printf 'sender pid %s exited before shutdown CPU sample completed\n' "$sender_pid"
fi
wait_shutdown 2 10
shutdown_immediate
wait_shutdown "" 10

if [ "$delta_ticks" -gt "$threshold_ticks" ] || [ "$shutdown_delta_ticks" -gt "$threshold_ticks" ]; then
	printf 'FAIL: omrelp consumed too much CPU after OpenSSL TLS auth failure without tls.caCert\n'
	error_exit 1
fi

exit_test
