#!/bin/bash
# Validate-mode classification for named port-0 listener retirement.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-retire-rainerscript.sh
