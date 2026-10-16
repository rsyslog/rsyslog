#!/bin/bash
# Native-YAML validate-mode oracle: report fixed-numeric addition support
# while the port remains reserved, without binding or advancing generation.
# SPDX-License-Identifier: Apache-2.0
export RSYSLOG_RELOAD_CONFIG_FORMAT=yaml
export RSYSLOG_RELOAD_ENDPOINT_MODE=validate
. ${srcdir:-.}/global-reloadonhup-imtcp-add-plain-rainerscript.sh
