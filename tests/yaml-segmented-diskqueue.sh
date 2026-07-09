#!/bin/bash
# Verify YAML config accepts and instantiates queue.type="segmentedDisk" through
# the shared queue parameter backend. The oracle is sequence correctness after
# runtime enqueue/drain through a YAML-configured main queue.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check

generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
rm -f "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'global:'
add_yaml_conf '  workDirectory: "'${RSYSLOG_DYNNAME}'.spool"'
add_yaml_conf 'mainqueue:'
add_yaml_conf '  queue.type: segmentedDisk'
add_yaml_conf '  queue.filename: mainq'
add_yaml_conf '  queue.maxFileSize: 8k'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg:F,58:2%\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      :msg, contains, "msgnum:" action(type="omfile"'
add_yaml_conf '          template="outfmt" file="'${RSYSLOG_OUT_LOG}'")'

startup
tcpflood -m "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
