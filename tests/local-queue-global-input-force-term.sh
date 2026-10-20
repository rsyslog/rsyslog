#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the same capacity/wake/accepted-handoff oracle at a global-scope graph
# boundary. The local action keeps the graph enabled while main admission uses
# the ordinary global single/multi enqueue reference contract.
export LOCAL_QUEUE_INPUT_TERM_SCOPE=global
. ${srcdir:=.}/local-queue-input-force-term.sh
