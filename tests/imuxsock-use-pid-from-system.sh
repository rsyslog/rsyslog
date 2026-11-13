#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Regression coverage for imuxsock UsePIDFromSystem TAG rewriting. Empty TAGs
# must append the credential PID without writing before the TAG buffer; normal,
# colon-terminated, and existing-PID TAGs must retain their established shape.
# Near-capacity TAGs must accept a suffix when it fits, while a 511-byte TAG
# must remain unchanged. Exact output after synchronized shutdown is the oracle.
. "${srcdir:=.}/diag.sh" init

export NUMMESSAGES=6
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock"
      Socket="'$RSYSLOG_DYNNAME'-testbench_socket"
      UsePIDFromSystem="on")

template(name="outfmt" type="string" string="%syslogtag%|%msg%\n")
*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

socket="$RSYSLOG_DYNNAME-testbench_socket"
./syslog_caller -m1 -u "$socket" -R '<13> empty'
./syslog_caller -m1 -u "$socket" -R '<13>tag: colon'
./syslog_caller -m1 -u "$socket" -R '<13>tag no-colon'
./syslog_caller -m1 -u "$socket" -R '<13>tag[123]: existing-pid'
near_tag="$(printf 'A%.0s' {1..500})"
./syslog_caller -m1 -u "$socket" -R "<13>${near_tag} near-capacity"
full_tag="${near_tag}AAAAAAAAAAA"
./syslog_caller -m1 -u "$socket" -R "<13>${full_tag} full-capacity"

shutdown_when_empty
wait_shutdown

content_check --regex '^\[[0-9]+\]:\| empty$'
content_check --regex '^tag\[[0-9]+\]:\| colon$'
content_check --regex '^tag\[[0-9]+\]:\| no-colon$'
content_check --regex '^tag\[[0-9]+\]:\| existing-pid$'
content_check --regex "^${near_tag}\[[0-9]+\]:\\| near-capacity$"
content_check "${full_tag}| full-capacity"
exit_test
