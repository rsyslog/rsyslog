#!/bin/bash
# Validate-mode parity for dynamic port-0 additions and fixed-numeric bind
# conflicts. The base scenario checks report-only classification, unchanged
# generation/baseline, and existing sessions/accepts; fixed numeric additions
# are supported but are never bound in validate mode.
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-add-yaml.sh
