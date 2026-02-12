#!/bin/bash
# Local simulation of GitHub Actions VictoriaMetrics test
# Usage: ./test-vm-push-local.sh

set -e
rc=0

cleanup() {
    docker rm -f vm-test >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "=== Starting VictoriaMetrics container ==="
docker run -d --name vm-test -p 8428:8428 victoriametrics/victoria-metrics:latest

echo "Waiting for VM to start..."
for i in {1..30}; do
    if curl -sf http://localhost:8428/ > /dev/null 2>&1; then
        echo "✓ VictoriaMetrics is ready"
        break
    fi
    echo "Attempt $i: waiting..."
    sleep 2
done

echo ""
echo "=== Running rsyslog impstats push test ==="

export RSYSLOG_DEV_CONTAINER=rsyslog/rsyslog_dev_base_ubuntu:24.04
export RSYSLOG_TESTBENCH_EXTERNAL_VM_URL=http://127.0.0.1:8428
export RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE="--enable-testbench --enable-omstdout --enable-imdiag --enable-impstats --enable-impstats-push --disable-imfile --disable-imfile-tests --disable-default-tests"
export CI_MAKE_OPT="-j8"
export CI_MAKE_CHECK_OPT="-j4"
export CI_CHECK_CMD="check"
export DOCKER_RUN_EXTRA_OPTS="--network host"
export VERBOSE=1

# Run the test via devcontainer (mirrors GitHub Actions)
devtools/devcontainer.sh --rm devtools/run-ci.sh

echo ""
echo "=== Validating metrics in VictoriaMetrics ==="
sleep 5

response=$(curl -s "http://localhost:8428/api/v1/label/__name__/values")
echo "Available metrics:"
echo "$response" | jq -r '.data[]' | grep "^pstat_" || echo "⚠ No pstat metrics found"

# Try querying a specific metric
pstat_count=$(echo "$response" | jq -r '.data[]' | grep -c "^pstat_" || echo 0)
echo "Found $pstat_count pstat metrics"

if [ "$pstat_count" -ge 3 ]; then
    echo "✓ Test PASSED - metrics found in VictoriaMetrics"
else
    echo "✗ Test FAILED - insufficient metrics"
    docker logs vm-test
    rc=1
fi

echo ""
echo "=== Cleanup ==="
cleanup

echo "Done!"
exit $rc
