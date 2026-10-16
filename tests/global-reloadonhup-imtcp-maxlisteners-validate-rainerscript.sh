#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Report listener capacity eligibility without activating or advancing baseline.
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:=.}/global-reloadonhup-imtcp-maxlisteners-rainerscript.sh
