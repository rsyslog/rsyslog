#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-16 by Rainer Gerhards, released under ASL 2.0
# Run the maxCardinality bound oracle with YAML action parameters to check
# that the new parameter has the same effect in the YAML frontend.
export MMCOUNT_TEST_YAML=1
. ${srcdir:=.}/mmcount-maxcardinality.sh
