#!/bin/bash
# Validate-mode parity for the deliberate existing-listener-only milestone.
# The base scenario asserts reported_only + restart_required for unsupported
# endpoints, unchanged generation/baseline, absent candidate binding, and old
# sessions/accepts remaining usable. Supported profile changes are report-only.
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-replace-rainerscript.sh
