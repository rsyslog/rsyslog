#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the Python 2 impstats analyzer with real log/CSV/output checks.
# The helper verifies percentage arithmetic, alarm boundaries and quiet zero
# controls while isolating the analyzer's fixed error-log destination. The
# ordinary testbench handles setup/cleanup; Python 2 absence is an explicit skip.
. ${srcdir:=.}/diag.sh init

bash "$srcdir/helpers/statslog-evicted-percent.sh"
result=$?
if [ "$result" -eq 77 ]; then
    skip_test
elif [ "$result" -ne 0 ]; then
    error_exit "$result"
fi
exit_test
