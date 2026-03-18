#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo This test must be run as root [connection interruption requires iptables]
echo "For variables to be passed use --preserve-env=AZURE_DCE_CLIENT_ID,AZURE_DCE_CLIENT_SECRET,AZURE_DCE_TENANT_ID,AZURE_DCE_URL,AZURE_DCE_DCR_ID,AZURE_DCE_TABLE_NAME"
if [ "$EUID" -ne 0 ]; then
	exit 77
fi

. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/omazuredce-env.sh

require_plugin "omazuredce"
omazuredce_require_env

if ! command -v iptables >/dev/null 2>&1; then
	echo "SKIP: iptables command not available - SKIPPING"
	exit 77
fi
if ! command -v ip6tables >/dev/null 2>&1; then
	ip6tables=
fi

export NUMMESSAGES=400
export interrupt_port="$(omazuredce_url_port)"
export TEST_TIMEOUT_WAIT=180
interrupt_status_file="$RSYSLOG_DYNNAME.interrupt.status"
interrupt_detail_file="$RSYSLOG_DYNNAME.interrupt.detail"
interrupt_ipv4_hosts="$(omazuredce_url_ips_v4)"
interrupt_ipv6_hosts="$(omazuredce_url_ips_v6)"

if [[ -z "$interrupt_ipv4_hosts" && -z "$interrupt_ipv6_hosts" ]]; then
	echo "SKIP: unable to resolve AZURE_DCE_URL host to an IP address - SKIPPING"
	exit 77
fi
if [[ -n "$interrupt_ipv6_hosts" && -z "${ip6tables:-}" ]]; then
	if [[ -n "$interrupt_ipv4_hosts" ]]; then
		echo "NOTICE: ip6tables command not available, skipping IPv6 interruption targets"
		interrupt_ipv6_hosts=
	else
		echo "SKIP: ip6tables command not available for IPv6-only interruption target - SKIPPING"
		exit 77
	fi
fi

omazuredce_post_count() {
	if [ -f "$RSYSLOG_OUT_LOG" ]; then
		grep -c -F "omazuredce: posted batch records=" < "$RSYSLOG_OUT_LOG"
	else
		echo 0
	fi
}

omazuredce_fail_count() {
	if [ -f "$RSYSLOG_OUT_LOG" ]; then
		grep -c -F "omazuredce: batch post failed:" < "$RSYSLOG_OUT_LOG"
	else
		echo 0
	fi
}

omazuredce_add_reject_rules() {
	local host

	for host in $interrupt_ipv4_hosts; do
		iptables -I OUTPUT -d "$host" -p tcp --dport "$interrupt_port" -j REJECT --reject-with tcp-reset || return 1
		iptables -I INPUT -s "$host" -p tcp --sport "$interrupt_port" -j REJECT --reject-with tcp-reset || return 1
	done
	for host in $interrupt_ipv6_hosts; do
		ip6tables -I OUTPUT -d "$host" -p tcp --dport "$interrupt_port" -j REJECT --reject-with tcp-reset || return 1
		ip6tables -I INPUT -s "$host" -p tcp --sport "$interrupt_port" -j REJECT --reject-with tcp-reset || return 1
	done
}

omazuredce_del_reject_rules() {
	local host

	for host in $interrupt_ipv4_hosts; do
		iptables -D OUTPUT -d "$host" -p tcp --dport "$interrupt_port" -j REJECT --reject-with tcp-reset 2>/dev/null || true
		iptables -D INPUT -s "$host" -p tcp --sport "$interrupt_port" -j REJECT --reject-with tcp-reset 2>/dev/null || true
	done
	for host in $interrupt_ipv6_hosts; do
		ip6tables -D OUTPUT -d "$host" -p tcp --dport "$interrupt_port" -j REJECT --reject-with tcp-reset 2>/dev/null || true
		ip6tables -D INPUT -s "$host" -p tcp --sport "$interrupt_port" -j REJECT --reject-with tcp-reset 2>/dev/null || true
	done
}

omazuredce_cleanup_interrupt() {
	if [ -n "${interrupt_bg_pid:-}" ]; then
		kill "$interrupt_bg_pid" 2>/dev/null || true
		wait "$interrupt_bg_pid" 2>/dev/null || true
	fi
	omazuredce_del_reject_rules
}

omazuredce_interrupt_traffic() {
	local tries

	: >"$interrupt_status_file"
	tries=0
	while [ "$tries" -lt 8 ]; do
		if [ "$(omazuredce_post_count)" -ge 2 ]; then
			echo "Injecting temporary iptables reject for port ${interrupt_port}"
			if ! omazuredce_add_reject_rules; then
				echo "skip" >"$interrupt_status_file"
				echo "unable to insert iptables/ip6tables reject rules" >"$interrupt_detail_file"
				omazuredce_del_reject_rules
				return
			fi
			$TESTTOOL_DIR/msleep 750
			omazuredce_del_reject_rules
			if [ "$(omazuredce_fail_count)" -gt 0 ]; then
				echo "done" >"$interrupt_status_file"
				return
			fi
			tries=$((tries + 1))
		else
			$TESTTOOL_DIR/msleep 100
		fi
	done
	echo "done" >"$interrupt_status_file"
}

trap omazuredce_cleanup_interrupt EXIT

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omazuredce/.libs/omazuredce")

template(name="tplAzureDceInterrupt" type="list" option.jsonftree="on") {
	property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="message" name="msg" format="jsonf")
}

local4.* action(
	type="omazuredce"
	template="tplAzureDceInterrupt"
	client_id="'$AZURE_DCE_CLIENT_ID'"
	client_secret=`echo $AZURE_DCE_CLIENT_SECRET`
	tenant_id="'$AZURE_DCE_TENANT_ID'"
	dce_url="'$AZURE_DCE_URL'"
	dcr_id="'$AZURE_DCE_DCR_ID'"
	table_name="'$AZURE_DCE_TABLE_NAME'"
	max_batch_bytes="4096"
	flush_timeout_ms="0"
	queue.type="FixedArray"
	queue.size="20000"
	queue.dequeueBatchSize="64"
	queue.minDequeueBatchSize="32"
	queue.minDequeueBatchSize.timeout="1000"
	queue.workerThreads="1"
	queue.workerThreadMinimumMessages="32"
	queue.timeoutWorkerthreadShutdown="60000"
	queue.timeoutEnqueue="2000"
	queue.timeoutshutdown="1000"
	action.resumeInterval="1"
	action.resumeRetryCount="-1"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg 0 $NUMMESSAGES
omazuredce_interrupt_traffic &
interrupt_bg_pid=$!
wait_file_lines --count-function omazuredce_post_count "$RSYSLOG_OUT_LOG" 5 "$TEST_TIMEOUT_WAIT"
wait "$interrupt_bg_pid"
interrupt_bg_pid=
if [[ -f "$interrupt_status_file" && "$(cat "$interrupt_status_file")" == "skip" ]]; then
	echo "SKIP: $(cat "$interrupt_detail_file") - SKIPPING"
	exit 77
fi
shutdown_when_empty
wait_shutdown

content_check "omazuredce: batch post failed:"
content_check "omazuredce: posted batch records="

post_count=$(grep -c -F "omazuredce: posted batch records=" < "$RSYSLOG_OUT_LOG")
if [ "${post_count:=0}" -lt 2 ]; then
	echo "FAIL: expected multiple successful omazuredce batch posts after interruption, got $post_count"
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
