#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML mixed-graph rejection must retain generation and old-root output.
export RSYSLOG_RELOAD_LOCAL_CONTROL=mixed
. ${srcdir:=.}/global-reloadonhup-on-yaml.sh
