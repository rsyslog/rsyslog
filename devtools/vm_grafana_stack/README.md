# VictoriaMetrics + Grafana (local test stack)

This is a **local testing aid** for inspecting impstats push output.
It is **not a production deployment**. It uses default credentials and minimal
configuration to keep the setup simple for developer workflows.

## What it provides
- VictoriaMetrics (remote write receiver + query UI)
- Grafana (pre-provisioned datasource + dashboard)

## Requirements
- Docker + docker compose

## Start (developer)
```bash
cd devtools/vm_grafana_stack
docker compose up -d
```

## Access (developer)
- VictoriaMetrics UI: http://localhost:8428
- Grafana: http://localhost:3000
  - user: `admin`
  - pass: `admin`

## Dashboard (developer)
The dashboard is auto-provisioned:
- `rsyslog impstats (VictoriaMetrics)`

If it doesn’t show up, restart Grafana:
```bash
docker compose restart grafana
```

## Populate data (developer)
Run the impstats push test against the local VictoriaMetrics:
```bash
RSYSLOG_TESTBENCH_EXTERNAL_VM_URL=http://localhost:8428 \
  ./tests/impstats-push-basic.sh
```
This command is safe to run repeatedly to refresh data.

## Tests and usage notes
- No automated tests **require** this stack.
- The stack is a manual aid for validating `impstats` push behavior and labels.
- Tests like `tests/impstats-push-basic.sh` can optionally use it via
  `RSYSLOG_TESTBENCH_EXTERNAL_VM_URL`.

## Review data (developer)
Useful curl queries:
```bash
# list all metric names
curl -s "http://localhost:8428/api/v1/label/__name__/values" | jq .

# query a known metric
curl -s "http://localhost:8428/api/v1/query?query=impstats_resource_usage_utime_total" | jq .

# inspect label sets for a specific series
curl -s "http://localhost:8428/api/v1/series?match[]=core_queue_main_Q_enqueued_total" | jq .
```

## AI agent quick run
```bash
cd devtools/vm_grafana_stack
docker compose up -d
RSYSLOG_TESTBENCH_EXTERNAL_VM_URL=http://localhost:8428 \
  ./tests/impstats-push-basic.sh
```

## Stop / cleanup
```bash
docker compose down -v
```

## Notes
- This stack is intentionally minimal and **not hardened**.
- It uses default admin credentials and no TLS.
