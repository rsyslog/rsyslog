# impstats Push Support (Prototype)

## Overview
This prototype adds push support to the impstats module, allowing rsyslog to send metrics directly to VictoriaMetrics (or any Prometheus Remote Write compatible endpoint).

**Current Implementation**: This prototype uses text parsing (Prometheus format output) as an interim solution. Stats are generated via `getAllStatsLines()` with `statsFmt_Prometheus`, then parsed and converted to protobuf.

**Future Optimization**: A native statsobj callback API will be added to provide direct access to raw uint64 counters, eliminating text parsing overhead.

## Architecture
- **Direct push from impstats**: Metrics are pushed directly from the impstats module using the Prometheus Remote Write protocol
- **Text parsing (interim)**: Current prototype parses Prometheus text format; future versions will use native statsobj callbacks
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

## Protocol Details
- Uses Prometheus Remote Write protocol (protobuf + Snappy compression)
- Required HTTP headers:
  - `Content-Type: application/x-protobuf`
  - `Content-Encoding: snappy`
  - `X-Prometheus-Remote-Write-Version: 0.1.0`
- Timestamps are milliseconds since Unix epoch
- Labels are sorted lexicographically (required by spec)
- `__name__` label contains the metric name

## Implementation Files
- **impstats_push.c/h**: Push orchestration (libcurl, statsobj iteration)
- **prometheus_remote_write.c/h**: Protobuf encoding and Snappy compression
- **remote.proto**: Prometheus Remote Write protobuf schema
- **impstats.c**: Integration hooks (#ifdef ENABLE_IMPSTATS_PUSH)
- **Makefile.am**: Conditional compilation rules
- **configure.ac**: Build system configuration

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
✅ Integration into impstats.c
⏳ Protobuf generation (requires protoc-c installation)
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
