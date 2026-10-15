#!/bin/bash
# Native YAML parity for fixed-numeric endpoint replacement and rollback.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
. ${srcdir:-.}/global-reloadonhup-imtcp-replace-plain-rainerscript.sh
