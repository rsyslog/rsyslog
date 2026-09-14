#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the latency helper's complete-line exact-ID oracle without rsyslog
# timing: good, missing, duplicate, and invalid sink records must remain
# distinguishable, and the fixed two-percent offered-rate limit is asserted.
. ${srcdir:=.}/diag.sh init
python3 "$srcdir/../benchmarks/queue-contention/latency-observer-selftest.py" || error_exit 1
exit_test
