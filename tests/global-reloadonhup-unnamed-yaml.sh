#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML parity for unnamed-action retention: status must reject HUP and
# the unchanged active root must deliver the subsequent numbered record.
export RSYSLOG_RELOAD_UNNAMED_CONTROL=1
. ${srcdir:=.}/global-reloadonhup-on-yaml.sh
