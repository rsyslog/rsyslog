#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML report-only listener capacity parity, retaining generation and traffic.
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
. ${srcdir:=.}/global-reloadonhup-imtcp-maxlisteners-rainerscript.sh
