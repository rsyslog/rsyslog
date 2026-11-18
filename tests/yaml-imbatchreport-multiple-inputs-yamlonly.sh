#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same nine-report instance-retention oracle via native YAML.
export IMBATCHREPORT_TEST_YAML=1
. ${srcdir:=.}/imbatchreport-multiple-inputs.sh
