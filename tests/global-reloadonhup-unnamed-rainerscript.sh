#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject removal of an active unnamed action by private optimization. The base
# scenario checks unchanged generation and delivery after the rejected HUP.
export RSYSLOG_RELOAD_UNNAMED_CONTROL=1
. ${srcdir:=.}/global-reloadonhup-on-rainerscript.sh
