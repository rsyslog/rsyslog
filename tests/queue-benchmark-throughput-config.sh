#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the throughput configuration generator retains the frozen S0 global
# main-queue stanza and emits local-only scope/front parameters only when the
# selected side is local. The arithmetic oracle proves that the documented
# 8x10K/1M/D1024 control and candidate have the same 1,088,192-slot bound.

set -euo pipefail

: "${srcdir:=$(dirname "$0")}"
command -v python3 >/dev/null 2>&1 || exit 77
harness="$srcdir/../benchmarks/queue-contention/trial-multi.sh"
driver="$srcdir/../benchmarks/queue-contention/compare.py"
common=(BENCH_CONFIG_ONLY=yes BENCH_MESSAGES=16 BENCH_CONNECTIONS=16 BENCH_PAYLOAD=512 BENCH_INPUT_WORKERS=8
    BENCH_DEQUEUE_BATCH_SIZE=1024 BENCH_WORKER_MINIMUM=1 BENCH_IMPSTATS=no)

global=$(env "${common[@]}" BENCH_SCOPE=global BENCH_QUEUE_SIZE=1088192 BENCH_CONSUMER_WORKERS=10 bash "$harness")
expected_global='main_queue_conf=main_queue(queue.type="FixedArray" queue.size="1088192"
    queue.workerThreads="10" queue.workerThreadMinimumMessages="1"
    queue.dequeueBatchSize="1024" queue.mutexContentionStats="off")'
if ! grep -Fqx 'scope=global' <<<"$global" || ! grep -Fqx 'total_slot_bound=1088192' <<<"$global" \
    || ! grep -Fqx 'backend_queue_size=1088192' <<<"$global" || ! grep -Fqx 'dequeue_batch_size=1024' <<<"$global" \
    || [[ "$global" != *"$expected_global"* ]] || grep -Fq 'queue.scope=' <<<"$global"; then
    echo "global configuration is not the frozen S0 control" >&2
    exit 1
fi

local=$(env "${common[@]}" BENCH_SCOPE=local BENCH_QUEUE_SIZE=1000000 BENCH_CONSUMER_WORKERS=2 \
    BENCH_FRONTEND_SIZE=10000 BENCH_MAX_FRONTENDS=8 bash "$harness")
if ! grep -Fqx 'scope=local' <<<"$local" || ! grep -Fqx 'total_slot_bound=1088192' <<<"$local" \
    || ! grep -Fqx 'frontend_size=10000' <<<"$local" || ! grep -Fqx 'max_frontends=8' <<<"$local" \
    || ! grep -Fq 'queue.scope="local" queue.local.frontendSize="10000" queue.local.maxFrontends="8"' <<<"$local"; then
    echo "local configuration or resource bound is incorrect" >&2
    exit 1
fi

if env "${common[@]}" BENCH_SCOPE=local BENCH_QUEUE_SIZE=1000000 bash "$harness" >/dev/null 2>&1; then
    echo "local configuration without frontend bounds unexpectedly passed" >&2
    exit 1
fi

plan=$(python3 "$driver" --before . --after . --output . --workload multi --messages 16 --pairs 1 \
    --input-workers 8 --connections 16 --payload 512 --worker-minimum 1 --dequeue-batch-size 1024 \
    --before-scope global --before-queue-size 1088192 --before-consumer-workers 10 \
    --after-scope local --after-queue-size 1000000 --after-consumer-workers 2 \
    --after-frontend-size 10000 --after-max-frontends 8 --print-configuration)
python3 -c '
import json
import sys
config = json.loads(sys.argv[1])["per_build_configuration"]
assert config["before"]["scope"] == "global"
assert config["before"]["total_slot_bound"] == 1088192
assert config["before"]["frontend_size"] is None
assert config["after"]["scope"] == "local"
assert config["after"]["total_slot_bound"] == 1088192
assert config["after"]["frontend_size"] == 10000
assert config["after"]["max_frontends"] == 8
' "$plan"
