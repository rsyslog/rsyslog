#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the helper fixtures, the driver's image-setup deadline, and one real
# imtcp/default-parser/omfile path. The image mock makes inspect, pull, and
# verification individually short but collectively exceed one timeout; its
# TimeoutError and absence of a trial prove the setup shares one deadline.
# Exact IDs after shutdown prove that RFC5424 MSG extraction reaches the sink;
# loose precision limits here test parser/oracle wiring, while benchmark runs
# use the stricter sub-millisecond invalidation limits. The trial is a fresh
# child testbench session so its diag.sh initialization cannot reuse this
# wrapper's generated-name state.
. ${srcdir:=.}/diag.sh init
command -v python3 >/dev/null 2>&1 || exit 77
python3 "$srcdir/../benchmarks/queue-contention/latency-observer-selftest.py" || error_exit 1
python3 - "$srcdir/../benchmarks/queue-contention/compare-latency.py" <<'PY' || error_exit 1
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    docker = root / 'docker'
    calls = root / 'docker.calls'
    docker.write_text("""#!/bin/sh
printf '%s\\n' \"$*\" >> \"$FAKE_DOCKER_LOG\"
if [ \"$1\" = image ] && [ \"$2\" = inspect ]; then
    sleep .12
    count=$(grep -c '^image inspect' \"$FAKE_DOCKER_LOG\")
    if [ \"$count\" -eq 1 ]; then exit 1; fi
    printf 'sha256:mock\\n'
    exit 0
fi
if [ \"$1\" = pull ]; then sleep .12; exit 0; fi
exit 9
""")
    docker.chmod(0o755)
    before, after, output = root / 'before', root / 'after', root / 'output'
    before.mkdir()
    after.mkdir()
    environment = os.environ.copy()
    environment.update({'PATH': str(root) + os.pathsep + os.environ['PATH'],
                        'FAKE_DOCKER_LOG': str(calls)})
    completed = subprocess.run([sys.executable, sys.argv[1], '--before', str(before), '--after', str(after),
                                '--output', str(output), '--pairs', '1', '--trial-timeout', '.3',
                                '--image', 'mock'], env=environment, capture_output=True, text=True, check=False)
    assert completed.returncode != 0
    assert json.loads((output / 'result.json').read_text())['failure']['type'] == 'TimeoutError'
    assert not any(line.startswith('run ') for line in calls.read_text().splitlines())
PY
require_plugin imtcp
metric_file="$PWD/$RSYSLOG_DYNNAME.latency.json"
(
    unset RSYSLOG_DYNNAME TESTCONF_NM
    BENCH_RATE_DRIFT_PERCENT=100 BENCH_MESSAGES=4 BENCH_CONNECTIONS=1 BENCH_OFFERED_RATE=100 BENCH_PAYLOAD=128 \
        BENCH_MAX_LATENESS_US=100000 BENCH_MAX_POLL_GAP_US=100000 \
        BENCH_METRIC_FILE="$metric_file" \
        bash "$srcdir/../benchmarks/queue-contention/trial-latency.sh"
) || error_exit 1
exit_test
