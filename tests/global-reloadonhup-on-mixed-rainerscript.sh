#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A global ruleset in a mixed graph must retain its root and generation on HUP.
export RSYSLOG_RELOAD_LOCAL_CONTROL=mixed
. ${srcdir:=.}/global-reloadonhup-on-rainerscript.sh
