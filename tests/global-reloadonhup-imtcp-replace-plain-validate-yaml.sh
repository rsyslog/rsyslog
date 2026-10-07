#!/bin/bash
# Native YAML validate-mode classification for fixed-numeric endpoint replacement.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-replace-plain-rainerscript.sh
