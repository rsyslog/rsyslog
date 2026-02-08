# impstats Push Support

## Overview
This feature adds native push support to the impstats module, allowing rsyslog to send internal statistics directly to VictoriaMetrics (or any Prometheus Remote Write compatible endpoint) without requiring external collectors.

**Implementation**: Uses native counter iteration API (`statsobj->GetAllCounters()`) to access raw counter values directly, eliminating text serialization and parsing overhead.

## Architecture
- **Direct push from impstats**: Metrics are pushed directly from the impstats module using the Prometheus Remote Write protocol
- **Native counter access**: Direct access to statsobj counters via callback API (no text parsing)
- **Efficient encoding**: Raw uint64 values → protobuf → Snappy compression → HTTP POST
- **Conditional build**: Feature is optional via `--enable-impstats-push` flag
- **Dependencies**: libcurl, libprotobuf-c, libsnappy

## Build Requirements
```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install libcurl4-openssl-dev libprotobuf-c-dev libsnappy-dev protobuf-c-compiler

# Configure and build
./autogen.sh
./configure --enable-impstats --enable-impstats-push
make
```

## Configuration Example
```
module(load="impstats"
    interval="10"
    format="json"
    
    # Push configuration
    push.url="http://localhost:8428/api/v1/write"
    push.labels=["instance=rsyslog-test", "env=dev"]
    push.timeout.ms="5000"
)
```

## Configuration Parameters
- **push.url**: VictoriaMetrics/Prometheus Remote Write endpoint URL (enables push when set)
- **push.labels**: Array of static labels to attach to all metrics (format: "key=value")
- **push.timeout.ms**: HTTP request timeout in milliseconds (default: 5000)
- **push.label.instance**: Add `instance=<hostname>` label (default: on)
- **push.label.job**: Add `job=<value>` label (default: "rsyslog"; set empty to disable)
- **push.label.origin**: Add `origin=<statsobj origin>` label (default: off)
- **push.label.name**: Add `name=<statsobj name>` label (default: off)
- **push.tls.cafile**: CA bundle file for TLS verification
- **push.tls.certfile**: Client certificate file for mTLS
- **push.tls.keyfile**: Client private key file for mTLS
- **push.tls.insecureSkipVerify**: Disable TLS verification (default: off)
- **push.batch.maxBytes**: Approximate maximum protobuf size per request (default: 0, disabled)
- **push.batch.maxSeries**: Maximum number of time series per request (default: 0, disabled)

## Protocol Details
- Uses Prometheus Remote Write protocol (protobuf + Snappy compression)
- Required HTTP headers:
  - `Content-Type: application/x-protobuf`
  - `Content-Encoding: snappy`
  - `X-Prometheus-Remote-Write-Version: 0.1.0`
- Timestamps are milliseconds since Unix epoch
- Labels are sorted lexicographically (required by spec)
- `__name__` label contains the metric name

## Metric Naming Convention

Metric names are constructed from statsobj components:

**Format**: `<origin>_<name>_<counter>_total`

Where:
- `origin`: Statsobj origin (e.g., "resource-usage", "core.queue", "imdiag")
- `name`: Statsobj instance name (omitted if empty)
- `counter`: Counter name (e.g., "utime", "enqueued", "submitted")

**Sanitization**: All non-alphanumeric characters (hyphens, dots, etc.) are replaced with underscores to comply with Prometheus naming requirements.

**Examples**:
- `resource-usage` origin, `utime` counter → `resource_usage_utime_total`
- `core.queue` origin, `main` name, `enqueued` counter → `core_queue_main_enqueued_total`
- `imdiag` origin, `0` name, `submitted` counter → `imdiag_0_submitted_total`

**Labels**: Supports static labels via `push.labels` and optional dynamic labels when enabled (`push.label.origin`, `push.label.name`, `push.label.instance`, `push.label.job`).

## Implementation Files
- **impstats_push.c/h**: Push orchestration (libcurl, native counter iteration)
- **prometheus_remote_write.c/h**: Protobuf encoding and Snappy compression
- **remote.proto**: Prometheus Remote Write protobuf schema
- **impstats.c**: Integration hooks (#ifdef ENABLE_IMPSTATS_PUSH)
- **Makefile.am**: Conditional compilation rules
- **configure.ac**: Build system configuration
- **runtime/statsobj.c/h**: Native counter iteration API (`GetAllCounters()`, interface v14)

## Technical Details

**Native Counter Access**:
- Uses `statsobj->GetAllCounters(callback, ctx)` to iterate all counters
- Callback receives: object name, origin, counter name, type, value, flags
- Direct uint64 access (no string serialization)
- Handles both atomic (`ctrType_IntCtr`) and non-atomic (`ctrType_Int`) counters safely

**Thread Safety**:
- Atomic counters (`ctrType_IntCtr`): Safe atomic read of uint64
- Gauge counters (`ctrType_Int`): Best-effort read without application lock
  - Values may be stale but acceptable for monitoring use cases
  - Most are gauges (queue size, open files) protected by app-level mutexes

**Performance**:
- Eliminates text generation and parsing overhead
- Direct memory-to-protobuf encoding
- Minimal allocations (metric names built on-demand, freed immediately)

## Testing
```bash
# Start VictoriaMetrics (Docker)
docker run -it --rm -p 8428:8428 victoriametrics/victoria-metrics

# Run rsyslog with test config
rsyslogd -f test.conf -n

# Query metrics
curl http://localhost:8428/api/v1/query -d 'query=rsyslog_resource_usage_utime'
```

## Current Status
✅ Build system (configure.ac, Makefile.am)
✅ Protobuf schema (remote.proto)
✅ Protobuf encoder (prometheus_remote_write.c/h)
✅ HTTP client (impstats_push.c/h)
✅ Native counter iteration API (statsobj v14)
✅ Metric name sanitization
✅ Integration into impstats.c
✅ Basic and VictoriaMetrics integration tests
✅ CI workflow (GitHub Actions)

## Future Enhancements
- Dynamic label injection (origin, object name as labels)
- Configurable metric naming templates
- Push batching/buffering for high-frequency stats
- Support for Prometheus text exposition format output
- Health endpoint (/metrics) alongside push
⏳ Compilation testing
⏳ Runtime testing with VictoriaMetrics

## Known Limitations
- Requires protoc-c at build time (not runtime)
- Push happens synchronously in stats generation thread
- No batching across multiple intervals
- No authentication support yet (basic/bearer token)

## Future Enhancements
- [ ] Add HTTP Basic Auth / Bearer token support
- [ ] Add TLS certificate verification options
- [ ] Add retry logic with exponential backoff
- [ ] Add push success/failure counters
- [ ] Consider async push (separate thread)
