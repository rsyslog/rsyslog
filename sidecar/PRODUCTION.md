# Production Deployment Guide

This guide covers production-ready deployment of the rsyslog Prometheus exporter using proper WSGI servers.

## Why Production WSGI Servers?

The built-in Werkzeug development server (`python3 rsyslog_exporter.py`) is **not suitable for production** because:

- Single-threaded request handling
- No process management or auto-restart
- Limited concurrency
- No graceful reload
- Not optimized for performance

**For production deployments, always use a production WSGI server like gunicorn.**

---

## Recommended: Gunicorn

### Installation

```bash
pip install gunicorn
```

Or use the provided `requirements.txt` which already includes gunicorn:

```bash
pip install -r requirements.txt
```

### Basic Usage

```bash
gunicorn --workers 1 -b 127.0.0.1:9898 rsyslog_exporter:application
```

### Production Configuration

```bash
gunicorn \
  --bind 127.0.0.1:9898 \
  --workers 1 \
  --threads 2 \
  --timeout 30 \
  --access-logfile /var/log/rsyslog-exporter/access.log \
  --error-logfile /var/log/rsyslog-exporter/error.log \
  --log-level info \
  --pid /run/rsyslog-exporter/rsyslog-exporter.pid \
  rsyslog_exporter:application
```

### Worker Count Recommendations

- **CPU-bound tasks**: `workers = (2 × CPU_cores) + 1`
- **I/O-bound tasks**: Start with 4-8 workers
- **This exporter**: 2-4 workers is typically sufficient (file mode)

**UDP Mode requires a single worker (workers=1):**

- The UDP listener must bind a single socket; multiple workers attempting to bind cause port clashes.
- This image and the provided systemd unit default to `--workers 1` to be safe.
- If you switch to file mode, you may increase workers for more HTTP concurrency.

**File Mode:** All workers share the same file reader (on demand), so worker count primarily affects HTTP concurrency.

### systemd Service (Gunicorn)

Create `/etc/systemd/system/rsyslog-exporter.service`:

```ini
[Unit]
Description=rsyslog Prometheus Exporter
After=network.target rsyslog.service

[Service]
Type=simple
User=rsyslog
Group=rsyslog
WorkingDirectory=/opt/rsyslog-exporter
Environment="IMPSTATS_MODE=udp"
Environment="IMPSTATS_FORMAT=json"
Environment="IMPSTATS_UDP_ADDR=127.0.0.1"
Environment="IMPSTATS_UDP_PORT=19090"
Environment="LISTEN_ADDR=127.0.0.1"
Environment="LISTEN_PORT=9898"
Environment="LOG_LEVEL=INFO"

ExecStart=/bin/sh -c '\
  /usr/local/bin/gunicorn \
    --bind ${LISTEN_ADDR}:${LISTEN_PORT} \
    --workers 1 \
    --threads 2 \
    --timeout 30 \
    --access-logfile /var/log/rsyslog-exporter/access.log \
    --error-logfile /var/log/rsyslog-exporter/error.log \
    --log-level info \
    --pid /run/rsyslog-exporter/rsyslog-exporter.pid \
    rsyslog_exporter:application'

ExecReload=/bin/kill -s HUP $MAINPID
KillMode=mixed
KillSignal=SIGTERM
PrivateTmp=true
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/rsyslog-exporter
RuntimeDirectory=rsyslog-exporter

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable rsyslog-exporter
sudo systemctl start rsyslog-exporter
sudo systemctl status rsyslog-exporter
```

---

## Alternative: uWSGI

### Installation

```bash
pip install uwsgi
```

### Basic Usage

```bash
uwsgi --http 127.0.0.1:9898 --wsgi-file rsyslog_exporter.py --callable application --master --processes 4 --threads 2
```

### Configuration File

Create `uwsgi.ini`:

```ini
[uwsgi]
http = 127.0.0.1:9898
wsgi-file = rsyslog_exporter.py
callable = application
master = true
processes = 4
threads = 2
vacuum = true
die-on-term = true
```

Run:

```bash
uwsgi --ini uwsgi.ini
```

---

## Docker Production Deployment

The included `Dockerfile` uses gunicorn by default:

```dockerfile
CMD ["gunicorn", "--bind", "0.0.0.0:9898", "--workers", "4", "--threads", "2", "--access-logfile", "-", "rsyslog_exporter:application"]
```

### Build and Run

```bash
docker build -t rsyslog-exporter .
docker run -d \
  --name rsyslog-exporter \
  -e IMPSTATS_MODE=udp \
  -e IMPSTATS_UDP_ADDR=0.0.0.0 \
  -e IMPSTATS_UDP_PORT=19090 \
  -p 19090:19090/udp \
  -p 9898:9898 \
  rsyslog-exporter
```

### Override Workers

```bash
docker run -d \
  --name rsyslog-exporter \
  -e IMPSTATS_MODE=udp \
  rsyslog-exporter \
  gunicorn --bind 0.0.0.0:9898 --workers 2 --threads 1 rsyslog_exporter:application
```

---

## Kubernetes Production Deployment

### Deployment with Gunicorn

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: rsyslog-exporter
spec:
  replicas: 1
  selector:
    matchLabels:
      app: rsyslog-exporter
  template:
    metadata:
      labels:
        app: rsyslog-exporter
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9898"
        prometheus.io/path: "/metrics"
    spec:
      containers:
      - name: exporter
        image: rsyslog-exporter:latest
        ports:
        - containerPort: 9898
          name: metrics
          protocol: TCP
        - containerPort: 19090
          name: udp-stats
          protocol: UDP
        env:
        - name: IMPSTATS_MODE
          value: "udp"
        - name: IMPSTATS_UDP_ADDR
          value: "0.0.0.0"
        - name: IMPSTATS_UDP_PORT
          value: "19090"
        - name: LISTEN_ADDR
          value: "0.0.0.0"
        - name: LISTEN_PORT
          value: "9898"
        - name: LOG_LEVEL
          value: "INFO"
        resources:
          limits:
            memory: "256Mi"
            cpu: "500m"
          requests:
            memory: "128Mi"
            cpu: "100m"
        livenessProbe:
          httpGet:
            path: /health
            port: 9898
          initialDelaySeconds: 10
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /health
            port: 9898
          initialDelaySeconds: 5
          periodSeconds: 10
```

---

## Performance Tuning

### Worker Configuration

**Scenario 1: Low-volume logging (<1K metrics)**
```bash
gunicorn -w 2 -b 127.0.0.1:9898 rsyslog_exporter:application
```

**Scenario 2 (File mode only): High-volume logging (>10K metrics)**
```bash
gunicorn -w 4 --threads 4 -b 127.0.0.1:9898 rsyslog_exporter:application
```

**Scenario 3 (File mode only): Very high scrape frequency**
```bash
gunicorn -w 8 --threads 2 -b 127.0.0.1:9898 rsyslog_exporter:application
```

### Timeouts

- **Short scrapes** (< 1s): `--timeout 10`
- **Normal scrapes** (1-5s): `--timeout 30` (default)
- **Large metric sets** (>5s): `--timeout 60`

### Memory Limits

Set `MAX_BURST_BUFFER_LINES` to prevent memory exhaustion during bursts:

```bash
export MAX_BURST_BUFFER_LINES=5000  # ~5MB for typical stats
gunicorn --workers 1 -b 127.0.0.1:9898 rsyslog_exporter:application  # increase only in file mode
```

---

## Monitoring Gunicorn

### Access Logs

```bash
tail -f /var/log/rsyslog-exporter/access.log
```

### Error Logs

```bash
tail -f /var/log/rsyslog-exporter/error.log
```

### Worker Stats

```bash
# Send USR1 to increase log level
kill -USR1 $(cat /var/run/rsyslog-exporter.pid)

# Send USR2 to decrease log level
kill -USR2 $(cat /var/run/rsyslog-exporter.pid)
```

### Graceful Reload

```bash
# Reload configuration without dropping requests
kill -HUP $(cat /var/run/rsyslog-exporter.pid)
```

### Health Monitoring & Alerts

**Monitor the exporter's health endpoint:**

```bash
# Check health and dropped messages
curl http://localhost:9898/health | jq '.dropped_messages, .metrics_count'
```

**Recommended Prometheus alerts:**

```yaml
# Alert if burst buffer is dropping messages (UDP mode)
- alert: RsyslogExporterDroppingStats
  expr: increase(rsyslog_exporter_dropped_messages[5m]) > 0
  annotations:
    summary: "Rsyslog exporter dropping stats due to buffer overflow"
    description: "Consider increasing MAX_BURST_BUFFER_LINES or reducing stats volume"

# Alert if metrics count drops unexpectedly
- alert: RsyslogExporterNoMetrics
  expr: rsyslog_exporter_metrics_count == 0
  for: 5m
  annotations:
    summary: "Rsyslog exporter has no metrics"
    description: "Check rsyslog impstats configuration and connectivity"
```

**Note:** The `/health` endpoint exposes `dropped_messages` counter only in UDP mode.

---

## Security Considerations

### 1. Bind to Localhost Only (Bare Metal)

```bash
gunicorn -b 127.0.0.1:9898 rsyslog_exporter:application
```

### 2. Use Reverse Proxy (Nginx/HAProxy)

nginx configuration:

```nginx
upstream rsyslog_exporter {
    server 127.0.0.1:9898;
}

server {
    listen 0.0.0.0:9898;
    
    location /metrics {
        proxy_pass http://rsyslog_exporter;
        proxy_set_header Host $host;
        allow 10.0.0.0/8;  # Allow only internal network
        deny all;
    }
    
    location /health {
        proxy_pass http://rsyslog_exporter;
        allow 127.0.0.1;  # Health checks from localhost only
        deny all;
    }
}
```

### 3. Enable Source IP Filtering

```bash
export ALLOWED_UDP_SOURCES="127.0.0.1,10.0.1.5"
gunicorn -b 127.0.0.1:9898 rsyslog_exporter:application
```

---

## Development vs Production

| Feature | Development (Werkzeug) | Production (Gunicorn) |
|---------|------------------------|----------------------|
| Command | `python3 rsyslog_exporter.py` | `gunicorn rsyslog_exporter:application` |
| Concurrency | Limited (threaded) | Multi-process + threaded |
| Performance | Low | High |
| Auto-reload | Optional | Graceful with HUP |
| Process management | None | Full (master/worker) |
| Logging | Basic | Advanced (access/error logs) |
| Production-ready | ❌ No | ✅ Yes |

**Always use gunicorn (or equivalent) in production environments.**

---

## Troubleshooting

### Issue: "Address already in use"

```bash
# Find and kill process using port 9898
sudo lsof -i :9898
sudo kill -9 <PID>
```

### Issue: Workers dying unexpectedly

Check memory:
```bash
journalctl -u rsyslog-exporter -n 100
```

Reduce workers or increase system memory.

### Issue: Slow scrapes

Increase workers (file mode only):
```bash
gunicorn -w 8 rsyslog_exporter:application
```

Or reduce metric cardinality in rsyslog impstats.

### Issue: "No module named 'rsyslog_exporter'"

Ensure you're in the correct directory:
```bash
cd /opt/rsyslog-exporter
gunicorn rsyslog_exporter:application
```

---

## Summary

✅ **DO** use gunicorn in production  
✅ **DO** configure appropriate worker counts  
✅ **DO** bind to localhost for bare-metal deployments  
✅ **DO** use systemd for process management  
✅ **DO** monitor logs and resource usage  

❌ **DON'T** use `python3 rsyslog_exporter.py` in production  
❌ **DON'T** run with excessive workers (wastes memory)  
❌ **DON'T** expose publicly without authentication  
