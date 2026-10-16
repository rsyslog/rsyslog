#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject local-graph activation while retaining generation and old-root output.
export RSYSLOG_RELOAD_LOCAL_CONTROL=local
. ${srcdir:=.}/global-reloadonhup-on-rainerscript.sh
