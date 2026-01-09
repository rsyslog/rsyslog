# Security Best Practices

## Security Protections Implemented

### 1. Secure Defaults
- ✅ HTTP server binds to `127.0.0.1` (loopback only)
- ✅ UDP listener binds to `127.0.0.1` (loopback only)
- ✅ Configuration validation on startup
- ✅ Non-root user in Docker by default

### 2. DoS Protection
- ✅ **Burst buffer limit**: Max 10,000 lines per burst (configurable)
- ✅ **Message size limit**: Max 65,535 bytes per UDP packet
- ✅ **Dropped message tracking**: Monitored via `/health` endpoint
- ✅ **Thread-safe operations**: Proper locking prevents race conditions

### 3. Access Control
- ✅ **Source IP filtering**: Optional whitelist for UDP sources (`ALLOWED_UDP_SOURCES`)
- ✅ **Network binding control**: Independent bind addresses for UDP and HTTP
- ✅ **Read-only file access**: No write permissions needed

### 4. Input Validation
- ✅ Port numbers validated (1-65535)
- ✅ Mode validated (file/udp)
- ✅ Format validated (json/prometheus/cee)
- ✅ UTF-8 decoding with error handling

### 5. Monitoring & Visibility
- ✅ Dropped message counter exposed in health endpoint
- ✅ Source filtering status visible
- ✅ Detailed logging with security events

## Network Binding

### Default Configuration (Secure for Same-Host)

```bash
IMPSTATS_UDP_ADDR=127.0.0.1  # UDP listener (UDP mode)
LISTEN_ADDR=127.0.0.1        # HTTP metrics server
```

Both services bind to **loopback only** by default:
- ✅ rsyslog and Prometheus must be on same host
- ✅ No network exposure
- ✅ Minimal attack surface

### Common Deployment Patterns

#### 1. Same-Host Deployment (Most Secure)

**Scenario**: rsyslog, exporter, and Prometheus all on same host.

```bash
# Exporter config (defaults are correct)
IMPSTATS_MODE=udp
IMPSTATS_UDP_ADDR=127.0.0.1
LISTEN_ADDR=127.0.0.1
```

**Security level**: ⭐⭐⭐⭐⭐ Excellent

---

#### 2. Remote Prometheus via SSH Tunnel

**Scenario**: Exporter on rsyslog host, Prometheus on separate host.

**On rsyslog host:**
```bash
# Exporter stays on loopback
LISTEN_ADDR=127.0.0.1
LISTEN_PORT=9898
```

**On Prometheus host:**
```bash
# Create SSH tunnel
ssh -L 9898:localhost:9898 rsyslog-host -N

# Prometheus scrapes localhost:9898
```

**Security level**: ⭐⭐⭐⭐⭐ Excellent (encrypted tunnel)

---

#### 3. Specific Interface Binding

**Scenario**: Multi-homed host with VPN or internal management network.

```bash
# Bind to VPN interface only
IMPSTATS_UDP_ADDR=127.0.0.1      # rsyslog is local
LISTEN_ADDR=10.0.1.50            # VPN IP address
```

**Security level**: ⭐⭐⭐⭐ Good (requires proper network segmentation)

**Verify binding:**
```bash
ss -tulpn | grep 9898
# Should show: 10.0.1.50:9898, not 0.0.0.0:9898
```

---

#### 4. All Interfaces + Firewall

**Scenario**: Dynamic interfaces or simpler configuration with firewall protection.

```bash
LISTEN_ADDR=0.0.0.0  # Bind to all interfaces
```

**Required firewall rules:**
```bash
# UFW example
ufw allow from 10.0.0.0/8 to any port 9898
ufw deny 9898

# iptables example
iptables -A INPUT -p tcp --dport 9898 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 9898 -j DROP
```

**Security level**: ⭐⭐⭐ Adequate (relies on firewall)

---

#### 5. Container/Kubernetes Sidecar

**Scenario**: Running in containers with network isolation.

```bash
# Both can be 0.0.0.0 since container network provides isolation
IMPSTATS_UDP_ADDR=0.0.0.0
LISTEN_ADDR=0.0.0.0
```

**Kubernetes NetworkPolicy** (recommended):
```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: rsyslog-exporter-policy
spec:
  podSelector:
    matchLabels:
      app: rsyslog-exporter
  ingress:
  - from:
    - namespaceSelector:
        matchLabels:
          name: monitoring
    ports:
    - protocol: TCP
      port: 9898
```

**Security level**: ⭐⭐⭐⭐ Good (with NetworkPolicy)

---

## DoS Protection

### Buffer Overflow Protection

The exporter implements multiple layers of protection against memory exhaustion attacks:

**1. UDP Message Size Limit**
```bash
MAX_UDP_MESSAGE_SIZE=65535  # Default: max UDP packet size
```

**2. Burst Buffer Size Limit**
```bash
MAX_BURST_BUFFER_LINES=10000  # Default: 10k lines
```

**IMPORTANT:** If rsyslog emits more than 10,000 stat lines in a single burst (unusual for typical deployments), excess messages will be dropped. This is safe-by-default behavior to prevent memory exhaustion attacks.

**Signs you may need to increase this limit:**
- `/health` endpoint shows increasing `dropped_messages` count during normal operation
- Logs show "Burst buffer limit reached" warnings
- Your rsyslog instance has hundreds of actions/queues (>1000 stats lines per interval)

**To adjust:**
```bash
export MAX_BURST_BUFFER_LINES=20000  # Double the limit
```

If exceeded:
- New messages are dropped
- Warning logged
- `dropped_messages` counter incremented (visible in `/health`)

**Monitoring dropped messages:**
```bash
curl http://localhost:9898/health | jq '.dropped_messages'
```

### Source IP Filtering (UDP Mode)

When UDP listener must bind to `0.0.0.0`, use IP whitelisting:

```bash
# Only accept from localhost and specific IP
ALLOWED_UDP_SOURCES=127.0.0.1,10.0.1.50,192.168.1.100
```

**Verify in health endpoint:**
```bash
curl http://localhost:9898/health | jq '.source_filtering'
# Returns: "enabled" if filtering is active
```

**Rejected packets are:**
- Logged as warnings
- Counted in `dropped_messages`
- Not processed

---

## Additional Security Measures

### 1. Run as Non-Root User

**Systemd** (rsyslog-exporter.service):
```ini
[Service]
User=rsyslog
Group=rsyslog
NoNewPrivileges=true
```

**Docker** (already configured):
```dockerfile
USER rsyslog
```

### 2. File System Permissions

```bash
# Impstats file - read-only for exporter
chown rsyslog:rsyslog /var/log/rsyslog/impstats.json
chmod 644 /var/log/rsyslog/impstats.json

# Exporter script
chmod 755 /usr/local/bin/rsyslog_exporter.py
```

### 3. Resource Limits

**Systemd**:
```ini
[Service]
MemoryMax=128M
TasksMax=10
CPUQuota=50%
```

**Docker**:
```bash
docker run --memory=128m --cpus=0.5 ...
```

### 4. TLS/mTLS for Untrusted Networks

If metrics must traverse untrusted networks, use a reverse proxy:

**nginx with TLS**:
```nginx
server {
    listen 9898 ssl;
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    # Optional: client certificate authentication
    ssl_client_certificate /path/to/ca.pem;
    ssl_verify_client on;
    
    location / {
        proxy_pass http://127.0.0.1:9898;
    }
}
```

Then configure exporter to bind only to loopback:
```bash
LISTEN_ADDR=127.0.0.1  # Behind nginx
```

### 5. Monitoring and Alerts

Monitor the exporter itself:
```yaml
# Prometheus alert rules
groups:
- name: rsyslog_exporter
  rules:
  - alert: ExporterDown
    expr: up{job="rsyslog"} == 0
    for: 5m
  
  - alert: NoMetricsReceived
    expr: rate(rsyslog_core_action_processed[5m]) == 0
    for: 10m
```

## Security Checklist

- [ ] Exporter binds to appropriate interface (default 127.0.0.1 for bare-metal)
- [ ] Firewall rules restrict port 9898 (if LISTEN_ADDR=0.0.0.0)
- [ ] Exporter runs as non-root user
- [ ] Impstats file has minimal permissions (644)
- [ ] Resource limits configured (memory, CPU)
- [ ] TLS/mTLS if metrics cross network boundaries
- [ ] Network segmentation (metrics on management network)
- [ ] Monitoring alerts for exporter health
- [ ] Regular security updates (base image, Python packages)
- [ ] Audit logs reviewed periodically

## Incident Response

If unauthorized access is suspected:

1. **Immediate**: Stop exporter service
2. **Investigate**: Check system logs, network connections
3. **Remediate**: 
   - Change binding to loopback only
   - Add/update firewall rules
   - Rotate any credentials if applicable
4. **Monitor**: Watch for repeated access attempts
5. **Review**: Audit configuration and access controls

## Compliance Notes

### PCI-DSS
- Metrics may contain cardinality information about transaction processing
- Ensure metrics endpoint is on isolated network (Requirement 1.3)
- Encrypt if traversing untrusted networks (Requirement 4.1)

### HIPAA
- Impstats should not contain PHI, but verify in your environment
- Apply principle of least privilege for exporter access
- Maintain audit logs of metric access

### GDPR
- Metrics typically contain no personal data
- Document data flows in your DPA if required
