#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# chkseq -d permits repeated records without changing the sequence stride set
# by -i. Inclusive, stride-aligned ranges must accept duplicates at the first,
# interior and final records. Missing, reordered and extra distinct records
# must still fail; no daemon, timing threshold or external service is involved.
. ${srcdir:=.}/diag.sh init

failures=0
check_case() {
    local expected="$1" name="$2" start="$3" end="$4" stride="$5" duplicates="$6" records="$7"
    local actual=0
    local -a options=(-s "$start" -e "$end" -i "$stride")
    if [ "$duplicates" = yes ]; then
        options+=(-d)
    fi
    if printf '%s\n' "$records" | ./chkseq "${options[@]}"; then
        actual=0
    else
        actual=1
    fi
    if [ "$actual" -ne "$expected" ]; then
        echo "FAIL: $name (expected status $expected, actual $actual)"
        failures=$((failures + 1))
    fi
}

check_case 0 unit-stride 0 2 1 yes $'0\n0\n1\n1\n2\n2'
check_case 0 stride-two-plain 0 4 2 no $'0\n2\n4'
check_case 0 stride-two-first 0 4 2 yes $'0\n0\n2\n4'
check_case 0 stride-two-interior 0 4 2 yes $'0\n2\n2\n4'
check_case 0 stride-two-final 0 4 2 yes $'0\n2\n4\n4'
check_case 0 stride-three-offset 5 14 3 yes $'5\n5\n8\n11\n11\n14\n14'
check_case 1 strict-duplicates 0 4 2 no $'0\n0\n2\n4'
check_case 1 reordered-record 0 4 2 yes $'0\n4\n2\n4'
check_case 1 missing-record 0 4 2 yes $'0\n4'
check_case 1 extra-record 0 4 2 yes $'0\n2\n4\n6'

if [ "$failures" -ne 0 ]; then
    echo "$failures chkseq sequence cases failed"
    error_exit 1
fi
echo 'All chkseq duplicate/increment cases passed'
exit_test
