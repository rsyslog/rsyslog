#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# YAML companion for imuxsock-use-pid-from-system.sh. The YAML input enables
# UsePIDFromSystem and the same exact-output oracle covers empty, normal,
# colon-terminated, existing-PID, and capacity-boundary TAG rewriting after a
# synchronized shutdown.
# Each sender is waited for before starting the next: $! supplies its actual
# credential PID and preserves datagram order. cmp_exact rejects any wrong PID,
# TAG, missing/extra line, or reordered output.
. "${srcdir:=.}/diag.sh" init
require_yaml_support

if ! grep -q '^#define HAVE_SCM_CREDENTIALS 1' ../config.h; then
	echo "imuxsock UsePIDFromSystem requires SCM_CREDENTIALS"
	exit 77
fi

export NUMMESSAGES=6
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
include(file="'"${RSYSLOG_DYNNAME}"'.yaml")

template(name="outfmt" type="string" string="%syslogtag%|%msg%\n")
if $inputname == "imuxsock" then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'

cp -f "$srcdir/testsuites/yaml-imuxsock-use-pid-from-system.yaml" "${RSYSLOG_DYNNAME}.yaml"
sed -i "s|@SOCKET@|${RSYSLOG_DYNNAME}-testbench_socket|g" "${RSYSLOG_DYNNAME}.yaml"

startup

socket="$RSYSLOG_DYNNAME-testbench_socket"
./syslog_caller -m1 -u "$socket" -R '<13> empty' &
pid_1=$!
wait "$pid_1" || error_exit 1
./syslog_caller -m1 -u "$socket" -R '<13>tag: colon' &
pid_2=$!
wait "$pid_2" || error_exit 1
./syslog_caller -m1 -u "$socket" -R '<13>tag no-colon' &
pid_3=$!
wait "$pid_3" || error_exit 1
./syslog_caller -m1 -u "$socket" -R '<13>tag[123]: existing-pid' &
pid_4=$!
wait "$pid_4" || error_exit 1
near_tag="$(printf 'A%.0s' {1..500})"
./syslog_caller -m1 -u "$socket" -R "<13>${near_tag} near-capacity" &
pid_5=$!
wait "$pid_5" || error_exit 1
full_tag="${near_tag}AAAAAAAAAAA"
./syslog_caller -m1 -u "$socket" -R "<13>${full_tag} full-capacity" &
pid_6=$!
wait "$pid_6" || error_exit 1

shutdown_when_empty
wait_shutdown

export EXPECTED="[${pid_1}]:| empty
tag[${pid_2}]:| colon
tag[${pid_3}]:| no-colon
tag[${pid_4}]:| existing-pid
${near_tag}[${pid_5}]:| near-capacity
${full_tag}| full-capacity"
cmp_exact
exit_test
