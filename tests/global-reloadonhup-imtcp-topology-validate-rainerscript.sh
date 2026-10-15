#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the same pre-prepare topology rejection oracle with validate mode active.
export RELOAD_TOPOLOGY_MODE=validate
. ${srcdir:=.}/global-reloadonhup-imtcp-topology-rainerscript.sh
