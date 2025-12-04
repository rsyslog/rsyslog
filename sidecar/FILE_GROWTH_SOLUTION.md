# Handling impstats File Growth Issue

## The Problem

When using `module(load="impstats" log.file="...")`, rsyslog **appends** statistics to the file on each interval. This causes two issues:

1. **Unbounded file growth** - the file grows continuously
2. **Multiple snapshots** - the file contains historical data, not just current stats

## Solutions

The sidecar supports **two modes** to address this:

### Mode 1: UDP Listener (Recommended)

The sidecar listens on a UDP port and receives impstats directly from rsyslog via `omfwd`.

**Advantages:**
- ✅ No file growth issues
- ✅ Always has current stats in memory
- ✅ Works with all rsyslog versions
- ✅ Minimal overhead (localhost UDP)

**rsyslog configuration:**
```
ruleset(name="pstats") {
  action(type="omfwd" target="127.0.0.1" port="19090" protocol="udp")
}
module(load="impstats" interval="30" format="json" Ruleset="pstats" bracketing="on")
```

**Sidecar configuration:**
```bash
IMPSTATS_MODE=udp
IMPSTATS_UDP_ADDR=127.0.0.1
IMPSTATS_UDP_PORT=19090
IMPSTATS_FORMAT=json
STATS_COMPLETE_TIMEOUT=5  # Seconds to wait for burst completion
```

**How it works:**
1. rsyslog emits stats burst (multiple lines) via UDP to sidecar
2. Sidecar receives all messages in the burst
3. After `STATS_COMPLETE_TIMEOUT` seconds of no new messages, sidecar processes the complete burst
4. Metrics are updated atomically in memory
5. Prometheus scrapes the latest metrics

### Mode 2: File-based (For Future rsyslog)

When rsyslog adds support for overwriting the impstats file (instead of appending), file mode will work cleanly.

**rsyslog configuration (future):**
```
module(load="impstats" 
       interval="30" 
       format="json"
       log.file="/var/log/rsyslog/impstats.json"
       log.syslog="off"
       overwrite="on")  # Future feature
```

**Sidecar configuration:**
```bash
IMPSTATS_MODE=file
IMPSTATS_PATH=/var/log/rsyslog/impstats.json
IMPSTATS_FORMAT=json
```

**Current file mode limitations:**
- **File grows unbounded**: rsyslog appends on each interval (no overwrite)
- **Disk exhaustion risk**: Without log rotation, impstats file will fill disk
- **Sidecar reads on mtime change only**: May parse same historical data repeatedly
- **Performance degradation**: Parsing multi-GB files on every mtime change is expensive
- **Not recommended for production** until rsyslog implements `overwrite="on"` option

## Recommendation

**Use UDP mode** for production deployments until rsyslog implements file overwrite support.

**If you must use file mode:**
1. Implement external log rotation (logrotate with copytruncate)
2. Monitor disk space closely
3. Set `resetCounters="on"` in impstats config
4. Accept that historical data may be re-parsed after rotation

## Burst Handling in UDP Mode

rsyslog emits impstats as a burst of messages (one per metric group). The sidecar:

1. **Buffers incoming messages** as they arrive
2. **Waits for burst completion** - if no messages received for `STATS_COMPLETE_TIMEOUT` seconds (default 5), the burst is considered complete
3. **Processes atomically** - all buffered messages are parsed and metrics updated at once
4. **Serves latest state** - Prometheus scrapes always see the most recent complete snapshot

This ensures metrics are consistent (all from the same stats interval).

## Testing

### Test UDP Mode

```bash
# Terminal 1: Start sidecar in UDP mode
cd sidecar
IMPSTATS_MODE=udp \
IMPSTATS_UDP_PORT=19090 \
IMPSTATS_FORMAT=json \
./venv/bin/python3 rsyslog_exporter.py

# Terminal 2: Send test data via UDP
echo '{"name":"test","origin":"core","value":"123"}' | nc -u -w1 127.0.0.1 19090
sleep 6  # Wait for burst timeout
curl http://localhost:9898/health
curl http://localhost:9898/metrics | grep test
```

### Test File Mode

```bash
# Start sidecar in file mode
IMPSTATS_MODE=file \
IMPSTATS_PATH=examples/sample-impstats.json \
IMPSTATS_FORMAT=json \
./venv/bin/python3 rsyslog_exporter.py

# In another terminal
curl http://localhost:9898/metrics
```

## Docker Compose Example

See `docker-compose-udp.yml` for a complete example with rsyslog + UDP sidecar.
