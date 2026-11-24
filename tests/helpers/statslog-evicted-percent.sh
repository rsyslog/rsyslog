#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The Python 2 stats analyzer must report evictions as a percentage without
# integer division: 5/50 and 15/150 both mean 10 percent. Real log input and
# CSV counters prove parsing succeeded; stdout percentages and exit status
# prove reporting. A Python 2 launcher redirects only the fixed corrupt-log
# open to our temp directory, then execfile() runs the unchanged analyzer.
# Integral, alarm-boundary, below-alarm and zero controls
# preserve selection semantics. No daemon, services, background work or waits.
set -eu
export PYTHONDONTWRITEBYTECODE=1
source_root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
python=${STATSLOG_ANALYZER_PYTHON:-}
if [ -z "$python" ]; then
    for candidate in python2 python2.7; do
        if command -v "$candidate" >/dev/null 2>&1 &&
           "$candidate" -c 'import sys; sys.exit(0 if sys.version_info[0] == 2 else 1)'; then
            python=$candidate
            break
        fi
    done
fi
if [ -z "$python" ] || ! command -v "$python" >/dev/null 2>&1 ||
   ! "$python" -c 'import sys; sys.exit(0 if sys.version_info[0] == 2 else 1)'; then
    echo "SKIP: statslog analyzer requires its Python 2 interpreter"
    exit 77
fi
fixture_dir=$(mktemp -d "${TMPDIR:-/tmp}/statslog-percent.XXXXXX")
cleanup() {
    if [ "${STATSLOG_KEEP_FIXTURE:-0}" = 1 ]; then
        echo "preserved fixture: $fixture_dir"
    else
        rm -rf "$fixture_dir"
    fi
}
trap cleanup EXIT
cat > "$fixture_dir/isolated-analyzer.py" <<'PYTHON'
# SPDX-License-Identifier: Apache-2.0
# Redirect only the analyzer's fixed error-log destination; all other file
# operations and the executed analyzer source remain unchanged.
import __builtin__
import os
import sys
analyzer = sys.argv[1]
error_log = sys.argv[2]
sys.argv = [analyzer] + sys.argv[3:]
sys.path.insert(0, os.path.dirname(analyzer))
original_open = __builtin__.open

def isolated_open(path, *args, **kwargs):
    if path == "/tmp/statslog-analyzer.corrupted.log":
        path = error_log
    return original_open(path, *args, **kwargs)

__builtin__.open = isolated_open
execfile(analyzer, {"__name__": "__main__", "__file__": analyzer})
PYTHON
failures=0
run_case() {
    local name=$1 requests=$2 evicted=$3 expected=$4 alarm=${5:-5} rc
    local input="$fixture_dir/$name.log"
    printf 'Jun 26 14:21:44 host rsyslogd-pstats: case_%s: requests=%s evicted=%s \n' \
        "$name" "$requests" "$evicted" > "$input"
    if "$python" "$fixture_dir/isolated-analyzer.py" \
        "$source_root/plugins/impstats/statslog-analyzer.py" "$fixture_dir/$name.corrupted.log" \
        "--input=$input" "--evictedalarm=$alarm" \
        > "$fixture_dir/$name.stdout" 2> "$fixture_dir/$name.stderr"; then
        rc=0
    else
        rc=$?
    fi
    echo "$rc" > "$fixture_dir/$name.exit"
    if [ ! -f "$fixture_dir/$name.corrupted.log" ]; then
        echo "FAIL $name: isolated error-log destination was not used"
        failures=$((failures + 1))
    fi
    if [ "$rc" -ne 0 ]; then
        echo "FAIL $name: analyzer exited $rc"
        cat "$fixture_dir/$name.stderr"
        failures=$((failures + 1))
    elif [ "$expected" = quiet ]; then
        if grep -q 'File Requests were evicted' "$fixture_dir/$name.stdout"; then
            echo "FAIL $name: unexpected eviction alarm"
            failures=$((failures + 1))
        else
            echo "PASS $name: no eviction alarm"
        fi
    elif awk -v want="$expected" '
        $3 == "File" && $4 == "Requests" && $5 == "were" && $6 == "evicted" {
            gsub(/%$/, "", $2)
            if ($2 + 0 != want) bad = 1
            count++
        }
        END { exit (bad || count != 1) }
    ' "$fixture_dir/$name.stdout"; then
        echo "PASS $name: $expected percent"
    else
        echo "FAIL $name: expected $expected percent"
        cat "$fixture_dir/$name.stdout"
        failures=$((failures + 1))
    fi
    if ! grep -Fq ";host;case_$name;$requests;$evicted;" "$input-case_$name.csv"; then
        echo "FAIL $name: parsed CSV counters differ"
        failures=$((failures + 1))
    fi
}
run_case small 50 5 10
run_case nonmultiple 150 15 10
run_case integral 100 5 5
run_case threshold_equal 100 10 10 10
run_case threshold_above 100 10 quiet 11
run_case below_alarm 100 4 quiet
run_case zero_evictions 100 0 quiet
run_case zero_requests 0 0 quiet
[ "$failures" -eq 0 ] || exit 1
