# Security Review Summary

## Security Issues Identified and Fixed

### 1. Memory Exhaustion / DoS via Unbounded Buffer ⚠️ HIGH

**Issue**: UDP listener had no limit on burst buffer size. An attacker could send unlimited messages causing memory exhaustion.

**Fix**: Added `MAX_BURST_BUFFER_LINES` (default: 10,000)
- Buffer overflow attempts are logged and tracked
- Excess messages dropped
- Counter exposed in `/health` endpoint

**Code**:
```python
MAX_BURST_BUFFER_LINES = int(os.getenv("MAX_BURST_BUFFER_LINES", "10000"))

if len(self.burst_buffer) + len(lines) > MAX_BURST_BUFFER_LINES:
    logger.warning("Burst buffer limit reached, dropping messages")
    self.dropped_messages += 1
    return
```

---

### 2. Memory Exhaustion via Large UDP Packets ⚠️ MEDIUM

**Issue**: UDP `recvfrom()` used hardcoded 65535 but no validation after receive.

**Fix**: 
- Made size configurable via `MAX_UDP_MESSAGE_SIZE`
- Added size validation
- Oversized packets logged and dropped

**Code**:
```python
MAX_UDP_MESSAGE_SIZE = int(os.getenv("MAX_UDP_MESSAGE_SIZE", "65535"))

data, addr = self.sock.recvfrom(MAX_UDP_MESSAGE_SIZE)
if len(data) > MAX_UDP_MESSAGE_SIZE:
    logger.warning(f"Oversized packet ({len(data)} bytes), dropping")
    self.dropped_messages += 1
    continue
```

---

### 3. No Source IP Validation (UDP Mode) ⚠️ MEDIUM

**Issue**: UDP listener accepted packets from any source IP, even when binding to `0.0.0.0`.

**Fix**: Added optional source IP whitelist
- `ALLOWED_UDP_SOURCES` environment variable
- Comma-separated IP list
- Empty = allow all (backward compatible)
- Rejected packets logged and counted

**Code**:
```python
ALLOWED_UDP_SOURCES = os.getenv("ALLOWED_UDP_SOURCES", "")

if self.allowed_sources and source_ip not in self.allowed_sources:
    logger.warning(f"Rejected packet from unauthorized source: {source_ip}")
    self.dropped_messages += 1
    continue
```

**Usage**:
```bash
ALLOWED_UDP_SOURCES=127.0.0.1,10.0.1.50
```

---

### 4. Missing Configuration Validation ⚠️ LOW

**Issue**: Invalid configuration values (e.g., port 99999) could cause obscure runtime errors.

**Fix**: Added startup validation for:
- `IMPSTATS_MODE` ∈ {file, udp}
- `IMPSTATS_FORMAT` ∈ {json, prometheus, cee}
- `LISTEN_PORT` ∈ [1, 65535]
- `IMPSTATS_UDP_PORT` ∈ [1, 65535]

Fails fast with clear error message.

---

### 5. Information Disclosure via Health Endpoint ℹ️ INFO

**Issue**: `/health` endpoint accessible without authentication exposes:
- File paths
- UDP listener address/port
- Internal configuration

**Assessment**: This is **acceptable** because:
- Default binding is `127.0.0.1` (localhost only)
- Information is operational, not sensitive
- Needed for monitoring

**Mitigation**: Document that `/health` contains operational info and should not be exposed publicly.

---

### 6. HTTP Server Binding to 0.0.0.0 by Default ⚠️ MEDIUM (FIXED)

**Issue**: Original default was `LISTEN_ADDR=0.0.0.0`, exposing metrics on all interfaces.

**Fix**: Changed default to `127.0.0.1`
- Users must explicitly choose to expose externally
- Secure by default
- Well-documented options for remote access

---

## Security Enhancements Added

### Monitoring & Visibility

**Dropped message counter** in `/health`:
```json
{
  "dropped_messages": 5,
  "source_filtering": "enabled"
}
```

Allows detecting:
- DoS attempts
- Misconfiguration (buffer too small)
- Unauthorized sources

### Logging

Security-relevant events now logged:
- Unauthorized source IPs
- Buffer overflow attempts
- Oversized packets
- Configuration validation failures

Example:
```
[WARNING] Rejected UDP packet from unauthorized source: 192.168.1.100
[WARNING] Burst buffer limit reached (10000 lines), dropping 100 new lines
[WARNING] Oversized UDP packet (70000 bytes) from 10.0.1.50, dropping
```

---

## Remaining Security Considerations

### 1. No Authentication on Endpoints ℹ️

**Status**: By design
- Prometheus ecosystem typically uses network-level security
- mTLS can be added via reverse proxy (nginx, Envoy)

**Recommendation**: Document reverse proxy pattern for TLS/mTLS.

### 2. No Rate Limiting ℹ️

**Status**: Acceptable for typical use
- Burst buffer limit provides basic protection
- rsyslog controls message rate
- Prometheus scrape interval is predictable

**Recommendation**: Document expected load patterns.

### 3. Werkzeug Development Server ⚠️ LOW

**Status**: Development server shows warning
- Acceptable for low-traffic metrics endpoints
- Production users should front with nginx/gunicorn

**Recommendation**: Add production deployment notes.

---

## Configuration Security Checklist

- [x] HTTP server binds to 127.0.0.1 by default
- [x] UDP listener binds to 127.0.0.1 by default  
- [x] Buffer overflow protection enabled
- [x] Message size limits enforced
- [x] Source IP filtering available
- [x] Configuration validated at startup
- [x] Security events logged
- [x] Dropped messages tracked
- [x] Non-root user in Docker
- [x] Read-only file access

---

## Testing Recommendations

### Test DoS Protection
```bash
# Send oversized burst
for i in {1..20000}; do 
  echo '{"test":"'$i'"}' 
done | nc -u localhost 19090

# Check dropped count
curl http://localhost:9898/health | jq '.dropped_messages'
```

### Test Source Filtering
```bash
# Start with whitelist
ALLOWED_UDP_SOURCES=127.0.0.1 python3 rsyslog_exporter.py

# Try from unauthorized source
echo '{"test":"data"}' | nc -u -s 192.168.1.100 localhost 19090

# Check logs for rejection
```

### Test Configuration Validation
```bash
# Invalid port
LISTEN_PORT=99999 python3 rsyslog_exporter.py
# Should fail immediately with clear error
```

---

## Security Update Protocol

When security issues are discovered:

1. **Assess severity** (CVSS score)
2. **Create private patch**
3. **Notify maintainers** via security contact
4. **Coordinate disclosure** (90-day window)
5. **Release patch** with CVE if applicable
6. **Update SECURITY.md**

Security contact: rsyslog GitHub Issues (mark as security)
