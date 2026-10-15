#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML parity for listener capacity ownership and rollback controls.
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
. ${srcdir:=.}/global-reloadonhup-imtcp-maxlisteners-rainerscript.sh
