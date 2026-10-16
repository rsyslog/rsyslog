#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML must reject local graphs and retain generation and old-root output.
export RSYSLOG_RELOAD_LOCAL_CONTROL=local
. ${srcdir:=.}/global-reloadonhup-on-yaml.sh
