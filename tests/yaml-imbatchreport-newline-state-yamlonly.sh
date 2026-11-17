#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same exact batch-report newline/deduplication oracle through
# pure YAML input configuration; the shared scenario documents the controls.
export IMBATCHREPORT_TEST_YAML=1
. ${srcdir:=.}/imbatchreport-newline-state.sh
