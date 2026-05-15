#!/bin/bash
# added 2026-04-30 to guard issue #6612, released under ASL 2.0
#
# This test verifies that an omrelp sender without tls.caCert fails OpenSSL
# certificate validation cleanly: the action must be disabled after the auth
# error, and the sender must not busy-loop while handling the failed TLS
# connection or while shutting down afterward.
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

sample_cpu_activity() {
	local pid="$1"
	local interval_ms="$2"
	local sample_count="$3"
	local threshold_ticks="$4"
	local delta max_delta=0 over_threshold=0 samples=""
	local i

	for ((i = 0; i < sample_count; i++)); do
		delta=$(sample_cpu_delta_ticks "$pid" "$interval_ms") || return 1
		samples="${samples:+$samples,}$delta"
		if [ "$delta" -gt "$max_delta" ]; then
			max_delta="$delta"
		fi
		if [ "$delta" -gt "$threshold_ticks" ]; then
			over_threshold=$((over_threshold + 1))
		fi
	done

	printf '%s %d %d\n' "$samples" "$max_delta" "$over_threshold"
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
# The CPU samples are part of the test oracle, not debug noise.  TLS failure
# handling and shutdown can legitimately consume a short burst of CPU, so the
# test only fails if every sample in a window stays above the threshold.
sample_interval_ms=1200
sample_count=3
threshold_ticks=$((clk_tck * sample_interval_ms * 80 / 100000))
if [ "$threshold_ticks" -lt 10 ]; then
	threshold_ticks=10
fi

if ! cpu_activity=$(sample_cpu_activity "$sender_pid" "$sample_interval_ms" "$sample_count" "$threshold_ticks"); then
	error_exit 1 "sender process exited before CPU sampling started"
fi
read -r cpu_samples max_delta_ticks over_threshold_samples <<< "$cpu_activity"

printf 'sender pid %s CPU ticks while handling TLS auth failure: [%s], max %d, sustained threshold %d over %d samples\n' \
	"$sender_pid" "$cpu_samples" "$max_delta_ticks" "$threshold_ticks" "$sample_count"

content_check "DISABLING action" "$RSYSLOG_DYNNAME.errmsgs"

shutdown_immediate 2
$TESTTOOL_DIR/msleep 100
if shutdown_cpu_activity=$(sample_cpu_activity "$sender_pid" "$sample_interval_ms" "$sample_count" "$threshold_ticks"); then
	read -r shutdown_cpu_samples shutdown_max_delta_ticks shutdown_over_threshold_samples <<< "$shutdown_cpu_activity"
	printf 'sender pid %s CPU ticks after shutdown started: [%s], max %d, sustained threshold %d over %d samples\n' \
		"$sender_pid" "$shutdown_cpu_samples" "$shutdown_max_delta_ticks" "$threshold_ticks" "$sample_count"
else
	shutdown_over_threshold_samples=0
	printf 'sender pid %s exited before shutdown CPU sample completed\n' "$sender_pid"
fi
wait_shutdown 2 10
shutdown_immediate
wait_shutdown "" 10

if [ "$over_threshold_samples" -eq "$sample_count" ] || [ "$shutdown_over_threshold_samples" -eq "$sample_count" ]; then
	printf 'FAIL: omrelp consumed too much CPU after OpenSSL TLS auth failure without tls.caCert\n'
	error_exit 1
fi

exit_test
