#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject DNS-resolved, malformed, empty and legacy-policy candidates atomically.
# The shared fixture proves preserved generation and traffic on existing streams.
export RSYSLOG_ACL_RELOAD_REJECTIONS=1
. ${srcdir:=.}/global-reloadonhup-imtcp-allowedsender-yaml.sh
