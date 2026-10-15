#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Repeat capacity lifecycle checks with explicit unchanged socket backlog,
# allowing cross-bucket growth but rejecting a changed actual listen backlog.
export RSYSLOG_RELOAD_EXPLICIT_BACKLOG=1
. ${srcdir:=.}/global-reloadonhup-imtcp-maxsessions-rainerscript.sh
