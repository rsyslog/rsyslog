#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify YAML reaches the shared DA queue parameter backend. A fresh LinkedList
# main queue selects the fresh auto default with idle cleanup disabled. The
# absence of both marker and .segq proves selection remained fully
# unmaterialized before any spill; exact output proves memory-first operation.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20

generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'${RSYSLOG_OUT_LOG}'")
'
rm -f "${TESTCONF_NM}.yaml"
add_yaml_conf 'global:'
add_yaml_conf '  workDirectory: "'${RSYSLOG_DYNNAME}'.spool"'
add_yaml_conf 'mainqueue:'
add_yaml_conf '  queue.type: LinkedList'
add_yaml_conf '  queue.filename: mainq'
add_yaml_conf '  queue.diskQueueType: auto'
add_yaml_conf '  queue.diskQueueAutoUpgrade: off'
add_yaml_conf '  queue.diskQueueIdleTimeout: -1'

startup
injectmsg
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
[ ! -e "${RSYSLOG_DYNNAME}.spool/mainq.da-engine" ] || error_exit 1 "YAML DA marker created without a spill"
[ ! -e "${RSYSLOG_DYNNAME}.spool/mainq.segq" ] || error_exit 1 "YAML DA queue materialized without a spill"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
