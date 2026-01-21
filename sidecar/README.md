# rsyslog Prometheus Exporter

A lightweight Python sidecar that reads rsyslog `impstats` (file or UDP) and exposes Prometheus metrics at `/metrics`, plus a `/health` endpoint for operational checks.

## Features

- **Multiple format support**: JSON, native Prometheus, and CEE/Lumberjack formats
- **Lightweight**: Minimal dependencies, small memory footprint
- **Flexible deployment**: Runs as standalone process, systemd service, or container sidecar
- **Auto-refresh**: Monitors impstats file and reloads when updated
- **Health checks**: Built-in `/health` endpoint for monitoring
- **Production-ready**: Uses gunicorn WSGI server for production deployments
- **Secure defaults**: Binds to localhost, supports source IP filtering

> Note on UDP mode: For stability, UDP mode requires a single Gunicorn worker (`--workers 1`). The provided Dockerfile and systemd unit already default to one worker. Increase workers only when using file mode.

> **⚠️ Production Deployments**: For production environments, **always use the gunicorn WSGI server** instead of the development server.

## Quick Start

If you just want a working setup on a host with rsyslog:
1. Run the installer (Option 2 below).
2. Let it deploy the sample rsyslog config when prompted.
3. Restart rsyslog and scrape `/metrics`.

### Prerequisites

- Python 3.8 or higher
- rsyslog configured with `impstats` module

### Installation Options

#### Option 1: Standalone Python Script (Development/Testing)

> **Note:** This uses the Werkzeug development server. For production, see Option 2 (systemd with gunicorn) below.

1. **Create a venv and install dependencies:**
   ```bash
   cd sidecar
   python3 -m venv .venv
   ./.venv/bin/pip install -r requirements.txt
   ```

2. **Configure rsyslog** to emit impstats (see [Configuration](#rsyslog-configuration) below)

3. **Run the exporter:**
   ```bash
   export IMPSTATS_PATH=/var/log/rsyslog/impstats.json
   export IMPSTATS_FORMAT=json
   ./.venv/bin/python rsyslog_exporter.py
   ```
   
   You'll see a warning:
   ```
   ************************************************************
   DEVELOPMENT MODE - Not suitable for production!
   For production, use: gunicorn --workers 1 -b 127.0.0.1:9898 rsyslog_exporter:application
   ************************************************************
   ```

4. **Test the endpoint:**
   ```bash
   curl http://localhost:9898/metrics
   ```

#### Option 2: systemd Service with Gunicorn (Recommended for Production)

1. **Install via the helper script:**
   ```bash
   sudo ./install-service.sh --impstats-mode udp
   ```

   The installer will prompt to deploy a sample rsyslog impstats config to
   /etc/rsyslog.d/10-impstats.conf. If the file exists, it will ask whether
   to overwrite it. The deployed file uses `--impstats-udp-addr` and
   `--impstats-udp-port` for the target. If you did not set
   `--impstats-udp-addr`, the installer will use `--listen-addr` instead
   (except when it is 0.0.0.0).

2. **Check status:**
   ```bash
   sudo systemctl status rsyslog-exporter
   journalctl -u rsyslog-exporter -f
   ```

#### Option 3: Docker Container (Sidecar Pattern)

1. **Build the container:**
   ```bash
   cd sidecar
   docker build -t rsyslog-exporter:latest .
   ```

2. **Run with docker-compose:**
   ```bash
   docker-compose up -d
   ```

   This starts:
   - rsyslog container
   - rsyslog-exporter sidecar
   - (Optional) Prometheus for testing

3. **Or run standalone container:**
   ```bash
   docker run -d \
     --name rsyslog-exporter \
     -v /var/log/rsyslog:/var/log/rsyslog:ro \
     -e IMPSTATS_PATH=/var/log/rsyslog/impstats.json \
     -e IMPSTATS_FORMAT=json \
     -p 9898:9898 \
     rsyslog-exporter:latest
   ```

#### Option 4: Kubernetes Sidecar

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: rsyslog-with-exporter
spec:
  containers:
  # Main rsyslog container
  - name: rsyslog
    image: rsyslog/rsyslog:latest
    volumeMounts:
    - name: impstats
      mountPath: /var/log/rsyslog
  
  # Prometheus exporter sidecar
  - name: exporter
    image: rsyslog-exporter:latest
    env:
    - name: IMPSTATS_PATH
      value: /var/log/rsyslog/impstats.json
    - name: IMPSTATS_FORMAT
      value: json
    ports:
    - containerPort: 9898
      name: metrics
    volumeMounts:
    - name: impstats
      mountPath: /var/log/rsyslog
      readOnly: true
  
  volumes:
  - name: impstats
    emptyDir: {}
```

### Validation

Run parser validation (no HTTP server needed):
```bash
./tests/run-validate.sh
```

Run UDP burst test (expects exporter on http://localhost:19898):
```bash
./tests/run-test-udp.sh
```

## File Growth & Mode Choice

rsyslog `impstats` **appends** to the file each interval, so file mode can grow unbounded. For production, **UDP mode is recommended** to avoid file growth and ensure each scrape reflects a single stats burst.

**UDP mode (recommended):**
```
ruleset(name="pstats") {
   action(type="omfwd" target="127.0.0.1" port="19090" protocol="udp")
}
module(load="impstats" interval="30" format="json" Ruleset="pstats" bracketing="on")
```

**File mode (only if you manage rotation):**
```
module(load="impstats"
          interval="30"
          format="json"
          log.file="/var/log/rsyslog/impstats.json"
          log.syslog="off"
          resetCounters="off")
```

If you must use file mode, configure log rotation (e.g., `copytruncate`) and monitor disk usage.

## Production Checklist

- Use the systemd installer (Option 2) or run gunicorn directly.
- Keep `LISTEN_ADDR=127.0.0.1` unless you need remote scraping.
- Prefer UDP mode for stable, bounded memory and no file growth.
- If using file mode, set up log rotation and monitor disk usage.
- Restrict UDP sources with `ALLOWED_UDP_SOURCES` if packets are not loopback.

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `IMPSTATS_MODE` | `udp` | Input mode: `file` or `udp` |
| `IMPSTATS_PATH` | `/var/log/rsyslog/impstats.json` | Path to rsyslog impstats file (file mode) |
| `IMPSTATS_FORMAT` | `json` | Format: `json`, `prometheus`, or `cee` |
| `IMPSTATS_UDP_ADDR` | `127.0.0.1` | UDP bind address (UDP mode) - loopback only by default |
| `IMPSTATS_UDP_PORT` | `19090` | UDP port (UDP mode) |
| `STATS_COMPLETE_TIMEOUT` | `5` | Seconds to wait for burst completion (UDP mode) |
| `LISTEN_ADDR` | `127.0.0.1` | HTTP server bind address - **loopback only by default for security** |
| `LISTEN_PORT` | `9898` | HTTP server port |
| `LOG_LEVEL` | `INFO` | Logging level: `DEBUG`, `INFO`, `WARNING`, `ERROR` |
| `ALLOWED_UDP_SOURCES` | (unset) | Optional comma-separated IP allowlist for UDP packets (UDP mode). If unset or empty, all sources are allowed. If set to a non-empty string, packets from other sources are dropped and counted in `/health` as `dropped_messages`. |
| `MAX_BURST_BUFFER_LINES` | `10000` | Max lines buffered per stats burst (UDP mode). Excess lines are dropped to prevent DoS; track via `/health` `dropped_messages`. |
| `MAX_UDP_MESSAGE_SIZE` | `65535` | Max UDP message size accepted (bytes). Larger packets are dropped. |

## Security Highlights

- **Secure defaults**: HTTP and UDP bind to `127.0.0.1` by default.
- **DoS protection**: Burst buffer and UDP message size limits; dropped messages tracked in `/health`.
- **Source filtering**: Optional allowlist via `ALLOWED_UDP_SOURCES`.

**Expose metrics safely:**
- Same-host: keep `LISTEN_ADDR=127.0.0.1`.
- Remote Prometheus: bind to a specific internal/VPN IP or use firewall rules.
- Containers: `0.0.0.0` is acceptable with network isolation.

**Operational info exposure:** `/health` includes file paths and listener info.
Keep it on loopback or protect it with a reverse proxy/firewall.

**Security monitoring tips:**
```bash
# Check dropped UDP messages
curl http://localhost:9898/health | jq '.dropped_messages'

# Allowlist UDP sources (if binding to 0.0.0.0)
export ALLOWED_UDP_SOURCES=127.0.0.1,10.0.1.50
```

#### Security Considerations

**Default bindings are secure for same-host deployment:**
- UDP listener: `127.0.0.1` (accepts impstats from local rsyslog only)
- HTTP server: `127.0.0.1` (metrics accessible to local Prometheus only)

**Common deployment scenarios:**

1. **Same-host Prometheus (most secure):**
   ```bash
   IMPSTATS_UDP_ADDR=127.0.0.1  # rsyslog on same host
   LISTEN_ADDR=127.0.0.1        # Prometheus on same host
   ```

2. **Remote Prometheus on VPN/internal network:**
   ```bash
   IMPSTATS_UDP_ADDR=127.0.0.1      # rsyslog on same host
   LISTEN_ADDR=10.0.1.50            # Bind to VPN interface
   # Or use firewall rules with 0.0.0.0
   ```

3. **Container/Kubernetes sidecar:**
   ```bash
   IMPSTATS_UDP_ADDR=0.0.0.0        # Accept from rsyslog container
   LISTEN_ADDR=0.0.0.0              # Expose to Prometheus
   # Network isolation provided by container network
   ```

4. **Bare metal with firewall:**
   ```bash
   IMPSTATS_UDP_ADDR=127.0.0.1
   LISTEN_ADDR=0.0.0.0              # Bind to all, restrict with firewall
   # Firewall rule: allow port 9898 only from Prometheus servers
   ```

### rsyslog Configuration

Add the following to your rsyslog configuration (e.g., `/etc/rsyslog.d/impstats.conf`):

#### JSON Format (Recommended)

```
module(load="impstats" 
       interval="30" 
       format="json"
       log.file="/var/log/rsyslog/impstats.json"
       log.syslog="off"
       resetCounters="off")
```

#### Prometheus Native Format (rsyslog 8.2312.0+)

```
module(load="impstats" 
       interval="30" 
       format="prometheus"
       log.file="/var/log/rsyslog/impstats.prom"
       log.syslog="off")
```

Then configure the exporter:
```bash
export IMPSTATS_PATH=/var/log/rsyslog/impstats.prom
export IMPSTATS_FORMAT=prometheus
```

#### CEE Format (Legacy)

```
module(load="impstats" 
       interval="30" 
       format="cee"
       log.file="/var/log/rsyslog/impstats.json"
       log.syslog="off")
```

Then configure the exporter:
```bash
export IMPSTATS_FORMAT=cee
```

**Important**: Ensure the impstats file location is readable by the exporter process.

### Prometheus Scrape Configuration

Add to your `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'rsyslog'
    scrape_interval: 30s
    static_configs:
      - targets: ['localhost:9898']
        labels:
          instance: 'rsyslog-main'
          environment: 'production'
```

## Best Practices

### Update Intervals

- **rsyslog impstats interval**: 30 seconds (balance between freshness and overhead)
- **Prometheus scrape interval**: 30-60 seconds (match or slightly exceed impstats interval)
- **Exporter refresh**: On-demand per scrape (reloads file if changed)

### Metric Naming

The exporter converts rsyslog impstats metrics to Prometheus format with the following conventions:

- Prefix: `rsyslog_`
- Component: Derived from `origin` field (e.g., `core`, `imtcp`, `core.action`)
- Metric: Original field name, sanitized for Prometheus

Example mappings:

| rsyslog field | Prometheus metric |
|---------------|-------------------|
| `{"name":"global","origin":"core","utime":"123456"}` | `rsyslog_core_utime{rsyslog_component="core",rsyslog_resource="global"} 123456` |
| `{"name":"action 0","origin":"core.action","processed":"1000"}` | `rsyslog_core_action_processed{rsyslog_component="core.action",rsyslog_resource="action_0"} 1000` |

### Metric Types

The exporter automatically determines metric types based on field names:

- **Counters**: `processed`, `failed`, `submitted`, `utime`, `stime`, `called.*`, `bytes.*`
- **Gauges**: All other numeric fields (e.g., `size`, `maxrss`)

### Resource Limits

For production deployments:

- **Memory**: ~20-50 MB typical, ~100 MB with hundreds of metrics
- **CPU**: Negligible (file parsing on scrape only)
- **Disk I/O**: Minimal (reads impstats file on demand)

Recommended systemd limits:
```ini
MemoryMax=256M
TasksMax=10
```

### Production Notes

- Use gunicorn for production; UDP mode requires a single worker.
- For file mode, you can raise workers for HTTP concurrency.
- Keep `LISTEN_ADDR=127.0.0.1` unless protected by firewall or proxy.

Example production command:
```bash
./.venv/bin/python -m gunicorn \
   --bind 127.0.0.1:9898 \
   --workers 1 \
   --threads 2 \
   --timeout 30 \
   --access-logfile /var/log/rsyslog-exporter/access.log \
   --error-logfile /var/log/rsyslog-exporter/error.log \
   rsyslog_exporter:application
```

### Security

**Network Binding Security:**

By default, the exporter binds to loopback interfaces for security:
- **UDP listener**: `127.0.0.1` - accepts impstats only from local rsyslog
- **HTTP metrics**: `127.0.0.1` - serves metrics only to local Prometheus

This is **secure for single-host deployments**. For other scenarios:

**Remote Prometheus access:**
1. **Bind to specific interface** (recommended):
   ```bash
   LISTEN_ADDR=10.0.1.50  # Your VPN or internal network IP
   ```

2. **Use SSH tunneling**:
   ```bash
   # On Prometheus server
   ssh -L 9898:localhost:9898 rsyslog-host
   ```

3. **Bind to all + firewall** (use with caution):
   ```bash
   LISTEN_ADDR=0.0.0.0
   # Configure firewall to allow only Prometheus servers
   ```

**Additional security measures:**

1. **Run as non-root user** (default in Docker, configure in systemd)
2. **Read-only access** to impstats file (exporter only needs read)
3. **Firewall rules**: Restrict port 9898 to Prometheus servers only
4. **TLS/mTLS**: Use reverse proxy (nginx, Envoy) if metrics traverse untrusted networks
5. **Network segmentation**: Keep metrics endpoints on management/monitoring network

## Endpoints

### `/metrics`

Returns Prometheus text exposition format with all current metrics.

**Example:**
```
# HELP rsyslog_core_utime rsyslog metric rsyslog_core_utime
# TYPE rsyslog_core_utime counter
rsyslog_core_utime{rsyslog_component="core",rsyslog_resource="global"} 123456.0
# HELP rsyslog_core_action_processed rsyslog metric rsyslog_core_action_processed
# TYPE rsyslog_core_action_processed counter
rsyslog_core_action_processed{rsyslog_component="core.action",rsyslog_resource="action_0"} 4950.0
```

### `/health`

Returns JSON health status.

**Example:**
```json
{
  "status": "healthy",
  "uptime_seconds": 3600.5,
  "impstats_file": "/var/log/rsyslog/impstats.json",
  "impstats_format": "json",
  "metrics_count": 42,
  "parse_count": 120
}
```

## Testing

### Test with Sample Data

1. **Use provided sample files:**
   ```bash
   # JSON format
   export IMPSTATS_PATH=examples/sample-impstats.json
   export IMPSTATS_FORMAT=json
   python3 rsyslog_exporter.py
   ```

2. **Generate test data:**
   ```bash
   # Simulate rsyslog writing to impstats
   cat examples/sample-impstats.json > /tmp/test-impstats.json
   export IMPSTATS_PATH=/tmp/test-impstats.json
   python3 rsyslog_exporter.py
   ```

3. **Verify metrics:**
   ```bash
   curl http://localhost:9898/metrics
   curl http://localhost:9898/health
   ```

### Integration Testing

```bash
# Start rsyslog and exporter with docker-compose
docker-compose up -d

# Send test syslog messages
logger -n localhost -P 514 "Test message"

# Wait for impstats update (30s)
sleep 35

# Check metrics
curl http://localhost:9898/metrics | grep rsyslog

# View in Prometheus
open http://localhost:9090
```

## Troubleshooting

### Exporter shows 0 metrics

**Cause**: Impstats file not found or empty
**Solution**: 
- Check rsyslog configuration and restart rsyslog
- Verify file path and permissions
- Check rsyslog logs: `journalctl -u rsyslog -f`

### Metrics are stale

**Cause**: rsyslog not updating impstats file
**Solution**:
- Verify rsyslog `impstats` interval setting
- Check if rsyslog is processing messages
- Ensure impstats file location is writable by rsyslog

### Permission denied errors

**Cause**: Exporter cannot read impstats file
**Solution**:
```bash
# For systemd
sudo chown rsyslog:rsyslog /var/log/rsyslog/impstats.json
sudo chmod 644 /var/log/rsyslog/impstats.json

# For container
docker run -v /var/log/rsyslog:/var/log/rsyslog:ro ...
```

### High memory usage

**Cause**: Too many unique metric combinations (high cardinality)
**Solution**:
- Review rsyslog configuration for unnecessary stats
- Consider filtering metrics at Prometheus level
- Reduce Prometheus scrape frequency to lower parsing rate

## Advanced Usage

### Custom Metric Filtering

To filter specific metrics, modify the parser functions in `rsyslog_exporter.py`:

```python
def parse_json_impstats(file_path: str) -> List[Metric]:
    # ... existing code ...
    
    # Skip certain origins
    if origin in ("debug", "internal"):
        continue
    
    # Only export specific metrics
    allowed_metrics = {"processed", "failed", "submitted"}
    if key not in allowed_metrics:
        continue
```

### Multiple Impstats Files

Run multiple exporter instances on different ports:

```bash
# Exporter 1
IMPSTATS_PATH=/var/log/rsyslog1/impstats.json LISTEN_PORT=9898 python3 rsyslog_exporter.py &

# Exporter 2
IMPSTATS_PATH=/var/log/rsyslog2/impstats.json LISTEN_PORT=9899 python3 rsyslog_exporter.py &
```

## Future Enhancements (Go Version)

A Go-based version is planned for future development with these advantages:

- **Smaller footprint**: ~5-10 MB binary vs ~30-50 MB Python
- **Better performance**: Lower latency, lower CPU usage
- **Static binary**: No runtime dependencies
- **Easier distribution**: Single binary for all platforms

The Python version will remain supported and shares the same configuration contract, ensuring seamless migration when the Go version becomes available.

## Contributing

Contributions are welcome! Please follow the rsyslog project guidelines in `CONTRIBUTING.md`.

## License

This sidecar is part of the rsyslog project and follows the same licensing terms. See `COPYING` in the repository root.

## Support

- **Issues**: https://github.com/rsyslog/rsyslog/issues
- **Discussions**: https://github.com/rsyslog/rsyslog/discussions
- **Documentation**: https://www.rsyslog.com/doc/

## See Also

- [rsyslog impstats module documentation](https://www.rsyslog.com/doc/master/configuration/modules/impstats.html)
- [Prometheus exposition formats](https://prometheus.io/docs/instrumenting/exposition_formats/)
- [Grafana dashboards for rsyslog](https://grafana.com/grafana/dashboards/)
