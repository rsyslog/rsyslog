#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Failure diagnostics must include Automake FAIL, ERROR and XPASS results,
# while PASS, SKIP and XFAIL remain quiet. Generate real test-driver metadata
# instead of mirroring its result format. Also retain incomplete-test fallback
# and non-test-log exclusion. Unique markers and exact status headings are the
# oracle; no daemon, services, background helpers or timing waits are involved.
set -eu
source_root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
driver=${GATHER_CHECK_TEST_DRIVER:-"$source_root/test-driver"}
if [ ! -f "$driver" ]; then
    echo "SKIP: Automake test-driver is unavailable; run autoreconf first"
    exit 77
fi
fixture_dir=$(mktemp -d "${TMPDIR:-/tmp}/gather-check-fixture.XXXXXX")
collector_console=$(mktemp "${TMPDIR:-/tmp}/gather-check-console.XXXXXX")
cleanup() {
    if [ "${GATHER_CHECK_KEEP_FIXTURE:-0}" = 1 ]; then
        printf 'preserved fixture: %s\ncollector console: %s\n' "$fixture_dir" "$collector_console"
    else
        rm -rf "$fixture_dir"
        rm -f "$collector_console"
    fi
}
trap cleanup EXIT

make_result() {
    local name=$1 code=$2 expectation=$3 result=$4
    printf '#!/bin/sh\n# SPDX-License-Identifier: Apache-2.0\nprintf "marker-%s\\n"\nexit %s\n' \
        "$result" "$code" > "$fixture_dir/$name.sh"
    (
        cd "$fixture_dir"
        sh "$driver" --test-name "$name" --log-file "$name.log" \
            --trs-file "$name.trs" --expect-failure "$expectation" \
            --enable-hard-errors yes -- sh "./$name.sh"
    )
    if ! grep -qx ":test-result: $result" "$fixture_dir/$name.trs"; then
        echo "invalid Automake fixture result for $name" >&2
        cat "$fixture_dir/$name.trs" >&2
        exit 1
    fi
}
make_result fail 1 no FAIL
make_result error 99 no ERROR
make_result xpass 0 yes XPASS
make_result pass 0 no PASS
make_result skip 77 no SKIP
make_result xfail 1 yes XFAIL
printf '#!/bin/sh\n# SPDX-License-Identifier: Apache-2.0\n' > "$fixture_dir/incomplete.sh"
printf 'marker-INCOMPLETE\n' > "$fixture_dir/incomplete.log"
printf 'marker-NONTEST\n' > "$fixture_dir/unrelated.log"
(
    cd "$fixture_dir"
    bash "$source_root/devtools/gather-check-logs.sh"
) > "$collector_console" 2>&1

failures=0
for spec in FAIL:fail ERROR:error XPASS:xpass; do
    result=${spec%%:*}
    name=${spec#*:}
    if ! grep -Fq "marker-$result" "$fixture_dir/failed-tests.log"; then
        echo "missing diagnostic body for $result" >&2
        failures=$((failures + 1))
    fi
    if ! grep -Fq "$result: ./$name" "$fixture_dir/failed-tests.log"; then
        echo "missing actual status heading for $result" >&2
        failures=$((failures + 1))
    fi
done
for result in PASS SKIP XFAIL NONTEST; do
    if grep -Fq "marker-$result" "$fixture_dir/failed-tests.log"; then
        echo "unexpected diagnostic body for $result" >&2
        failures=$((failures + 1))
    fi
done
if ! grep -Fq 'marker-INCOMPLETE' "$fixture_dir/failed-tests.log"; then
    echo "missing incomplete-test diagnostic fallback" >&2
    failures=$((failures + 1))
fi
if [ "$failures" -ne 0 ]; then
    cat "$fixture_dir/failed-tests.log" >&2
    cat "$collector_console" >&2
    exit 1
fi
echo 'PASS: failure statuses, quiet controls and incomplete fallback'
