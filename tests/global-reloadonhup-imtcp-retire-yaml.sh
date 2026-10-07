#!/bin/bash
# Native YAML parity for named port-0 listener retirement.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
. ${srcdir:-.}/global-reloadonhup-imtcp-retire-rainerscript.sh
