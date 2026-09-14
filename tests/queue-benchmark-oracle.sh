#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The queue benchmark must accept the exact ID range and reject both a missing
# ID and a duplicate. Exercise its real chkseq-based selftest through the single
# Automake test-owning subtree; no timing or daemon startup is part of this oracle.
. ${srcdir:=.}/diag.sh init
bash "$srcdir/../benchmarks/queue-contention/selftest.sh" ./chkseq || error_exit 1
exit_test
