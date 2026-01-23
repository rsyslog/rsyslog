# rsyslog Prometheus Exporter - Lightweight Sidecar Container

FROM python:3.12-slim

LABEL maintainer="rsyslog project"
LABEL description="Prometheus exporter sidecar for rsyslog impstats"

# Create non-root user
RUN groupadd -r rsyslog && useradd -r -g rsyslog rsyslog

# Set working directory
WORKDIR /app

# Copy requirements first for better layer caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy exporter script
COPY rsyslog_exporter.py .

# Make script executable
RUN chmod +x rsyslog_exporter.py

# Create directory for impstats file (will typically be mounted)
RUN mkdir -p /var/log/rsyslog && chown rsyslog:rsyslog /var/log/rsyslog

# Switch to non-root user
USER rsyslog

# Default environment variables (can be overridden)
ENV IMPSTATS_PATH=/var/log/rsyslog/impstats.json
ENV IMPSTATS_FORMAT=json
ENV LISTEN_ADDR=127.0.0.1
ENV LISTEN_PORT=9898
ENV LOG_LEVEL=INFO

# SECURITY NOTE: Default is loopback. Override with LISTEN_ADDR=0.0.0.0 in
# container deployments when you intend to expose /metrics.

# Expose metrics port
EXPOSE 9898

# Health check (respects LISTEN_ADDR/LISTEN_PORT)
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python3 - <<'PY' || exit 1
import http.client
import os
import sys

port = os.environ.get('LISTEN_PORT', '9898')
addr = os.environ.get('LISTEN_ADDR', '127.0.0.1')
if addr == '0.0.0.0':
    addr = '127.0.0.1'
try:
    conn = http.client.HTTPConnection(addr, int(port), timeout=4)
    conn.request("GET", "/health")
    res = conn.getresponse()
    if not (200 <= res.status < 300):
        sys.exit(1)
except Exception:
    sys.exit(1)
PY

# Run the exporter with gunicorn (production WSGI server)
# Default to a single worker to support UDP mode safely. Increase for file mode if needed.
CMD ["/bin/sh", "-c", "gunicorn --bind ${LISTEN_ADDR}:${LISTEN_PORT} --workers 1 --threads 2 --access-logfile - rsyslog_exporter:application"]
