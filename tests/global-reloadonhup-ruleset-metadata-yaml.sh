#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run native YAML metadata rejection, canonical no-op and supported-body
# controls with the same generation/delivery oracle as the RainerScript test.
export RSYSLOG_RELOAD_METADATA_YAML=1
. "${srcdir:=.}/global-reloadonhup-ruleset-metadata-rainerscript.sh"
