#!/bin/bash
# Native-YAML parity for fixed-numeric plain-ptcp addition, rollback, reuse,
# and live profile update. The shared scenario checks status and routed output.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
. ${srcdir:-.}/global-reloadonhup-imtcp-add-plain-rainerscript.sh
