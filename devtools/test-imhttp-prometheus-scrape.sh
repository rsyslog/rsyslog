#!/bin/bash
# Verify a live Prometheus container can scrape an imhttp endpoint from a
# separately networked rsyslog container. Target health and a queryable encoded
# metric are the oracle; all containers and temporary state are owned here.

set -euo pipefail

: "${RSYSLOG_PROMETHEUS_TARGET_IMAGE:=rsyslog/rsyslog_dev_base_ubuntu:26.04}"
: "${RSYSLOG_PROMETHEUS_IMAGE:=prom/prometheus:v3.7.3}"

readonly test_id="rsyslog-prometheus-${GITHUB_RUN_ID:-$$}-${GITHUB_RUN_ATTEMPT:-0}"
readonly network_name="${test_id}-network"
readonly target_name="${test_id}-target"
readonly prometheus_name="${test_id}-server"
readonly metric_name='U__action_2D_0_2D_builtin:omfile__processed__total'
tmpdir=$(mktemp -d)

cleanup() {
	status=$?
	if [ "$status" -ne 0 ]; then
		docker logs "$target_name" || true
		docker logs "$prometheus_name" || true
	fi
	docker rm -f "$target_name" "$prometheus_name" >/dev/null 2>&1 || true
	docker network rm "$network_name" >/dev/null 2>&1 || true
	rm -rf "$tmpdir"
	return "$status"
}
trap cleanup EXIT

cat >"$tmpdir/rsyslog.conf" <<'EOF'
global(workDirectory="/tmp")
module(load="/rsyslog/contrib/imhttp/.libs/imhttp" ports="9105" metricspath="/metrics")
action(type="omfile" file="/tmp/unused.log")
EOF
cat >"$tmpdir/prometheus.yml" <<'EOF'
global:
  scrape_interval: 1s
  evaluation_interval: 1s
scrape_configs:
  - job_name: rsyslog-imhttp
    static_configs:
      - targets: [rsyslog-metrics:9105]
EOF
chmod 644 "$tmpdir/prometheus.yml"

docker network create "$network_name" >/dev/null
docker run -d --name "$target_name" --network "$network_name" --network-alias rsyslog-metrics \
	--user "$(id -u):$(id -g)" -v "$PWD:/rsyslog" -v "$tmpdir:/test:ro" -w /rsyslog "$RSYSLOG_PROMETHEUS_TARGET_IMAGE" \
	bash -c 'exec ./tools/rsyslogd -n -M /rsyslog/runtime/.libs -f /test/rsyslog.conf' >/dev/null
docker run -d --name "$prometheus_name" --network "$network_name" -p 9090 \
	-v "$tmpdir/prometheus.yml:/etc/prometheus/prometheus.yml:ro" "$RSYSLOG_PROMETHEUS_IMAGE" \
	--config.file=/etc/prometheus/prometheus.yml --storage.tsdb.path=/tmp/prometheus >/dev/null

prometheus_port=$(docker port "$prometheus_name" 9090/tcp | sed -n 's/.*:\([0-9][0-9]*\)$/\1/p' | head -n1)
if [ -z "$prometheus_port" ]; then
	echo "Unable to determine Prometheus host port" >&2
	exit 1
fi

for _ in $(seq 1 60); do
	if curl -fsS "http://127.0.0.1:$prometheus_port/-/ready" >/dev/null 2>&1; then
		break
	fi
	sleep 1
done
curl -fsS "http://127.0.0.1:$prometheus_port/-/ready" >/dev/null

for _ in $(seq 1 60); do
	targets=$(curl -fsS "http://127.0.0.1:$prometheus_port/api/v1/targets")
	if jq -e '.data.activeTargets[] | select(.labels.job == "rsyslog-imhttp" and .health == "up")' \
		<<<"$targets" >/dev/null; then
		break
	fi
	sleep 1
done
targets=$(curl -fsS "http://127.0.0.1:$prometheus_port/api/v1/targets")
jq -e '.data.activeTargets[] | select(.labels.job == "rsyslog-imhttp" and .health == "up")' <<<"$targets" >/dev/null

query=$(curl -fsSG --data-urlencode "query=$metric_name" "http://127.0.0.1:$prometheus_port/api/v1/query")
jq -e '.status == "success" and (.data.result | length) > 0' <<<"$query" >/dev/null