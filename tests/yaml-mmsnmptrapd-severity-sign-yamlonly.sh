#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Apply the same numeric-code validation oracle through a YAML entry point and
# ruleset script; the common test documents the legacy module interface.
export MMSNMP_SEVERITY_YAML=1
. ${srcdir:=.}/mmsnmptrapd-severity-sign.sh
