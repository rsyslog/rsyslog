#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the exact scan-counter and parse-result oracle through pure YAML.
export MMJSONPARSE_STATS_YAML=1
. "${srcdir:=.}/mmjsonparse-find-json-scan-stats.sh"
