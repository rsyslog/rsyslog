#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# inputfilegen -s emits only complete records within the requested byte limit.
# An omitted -d and explicit -d0 both mean no extra payload, so they must have
# identical 17-byte records and floor counts at each boundary. Exact output
# and -M counts also cover positive padding; -s0 explicitly requests zero
# bytes (the unset size sentinel is -1), not unlimited generation.
. ${srcdir:=.}/diag.sh init

failures=0
check_size() {
    local name="$1" size="$2" extra="$3" count="$4"
    local output="$RSYSLOG_DYNNAME.$name.output" expected="$RSYSLOG_DYNNAME.$name.expected"
    local metadata="$RSYSLOG_DYNNAME.$name.count" padding="" i
    local -a options=(-s "$size" -M "$metadata")
    if [ "$extra" != default ]; then
        options+=(-d "$extra")
        padding=$(printf '%*s' "$extra" '' | tr ' ' X)
    fi
    if ! ./inputfilegen "${options[@]}" > "$output"; then
        echo "FAIL: $name generator failed"
        failures=$((failures + 1))
        return
    fi
    : > "$expected"
    for ((i = 0; i < count; ++i)); do
        printf 'msgnum:%08d:%s\n' "$i" "$padding" >> "$expected"
    done
    if ! cmp "$expected" "$output"; then
        echo "FAIL: $name expected $count complete records within $size bytes"
        failures=$((failures + 1))
    fi
    if [ "$(wc -c < "$output")" -gt "$size" ]; then
        echo "FAIL: $name exceeded requested $size-byte limit"
        failures=$((failures + 1))
    fi
    if [ "$(cat "$metadata")" != "$count" ]; then
        echo "FAIL: $name -M count differs from expected $count"
        failures=$((failures + 1))
    fi
}

for extra in default 0; do
    check_size "$extra-zero" 0 "$extra" 0
    check_size "$extra-short" 16 "$extra" 0
    check_size "$extra-exact" 17 "$extra" 1
    check_size "$extra-boundary" 32 "$extra" 1
    check_size "$extra-larger" 160 "$extra" 9
done
check_size padded-zero 0 7 0
check_size padded-short 23 7 0
check_size padded-exact 24 7 1
check_size padded-multiple 48 7 2
check_size padded-larger 160 7 6

if [ "$failures" -ne 0 ]; then
    echo "$failures inputfilegen size assertions failed"
    error_exit 1
fi
echo 'All inputfilegen size-limit cases passed'
exit_test
