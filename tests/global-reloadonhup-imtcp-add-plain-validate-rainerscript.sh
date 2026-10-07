#!/bin/bash
# Validate-mode oracle for fixed-numeric plain-ptcp addition: report support
# while the helper still owns the candidate port, without advancing generation.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-add-plain-rainerscript.sh
