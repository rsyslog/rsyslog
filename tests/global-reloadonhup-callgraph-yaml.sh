#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the same deterministic callgraph cutover oracle through YAML rulesets.
export CALLGRAPH_YAML=1
. ${srcdir:=.}/global-reloadonhup-callgraph-rainerscript.sh
