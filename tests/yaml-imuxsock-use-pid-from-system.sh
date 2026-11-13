#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion for imuxsock-use-pid-from-system.sh. The YAML input enables
# UsePIDFromSystem and the same exact-output oracle covers empty, normal,
# colon-terminated, existing-PID, and capacity-boundary TAG rewriting after a
# synchronized shutdown.
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
*.* action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'

cp -f "$srcdir/testsuites/yaml-imuxsock-use-pid-from-system.yaml" "${RSYSLOG_DYNNAME}.yaml"
sed -i "s|@SOCKET@|${RSYSLOG_DYNNAME}-testbench_socket|g" "${RSYSLOG_DYNNAME}.yaml"

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
