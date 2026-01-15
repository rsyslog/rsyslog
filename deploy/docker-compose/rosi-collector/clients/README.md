# Client Configuration Guide

This directory contains configuration files and instructions for setting up client hosts to send logs and metrics to your ROSI Collector server.

## Overview

To integrate a client host with the ROSI Collector, you need to configure:

1. **rsyslog** - Forward system logs to the collector (TCP port 10514)
2. **Prometheus Node Exporter** - Expose system metrics for collection (port 9100)

## Quick Start

### Option A: Automated Setup (Recommended)

1. **Configure rsyslog forwarding**:
   ```bash
   # Download from your ROSI Collector server
   wget https://YOUR_ROSI_DOMAIN/downloads/install-rsyslog-client.sh
   chmod +x install-rsyslog-client.sh
   sudo ./install-rsyslog-client.sh
   ```
   Replace `YOUR_ROSI_DOMAIN` with your ROSI Collector's domain name or IP address.

2. **Install Prometheus Node Exporter**:
   ```bash
   wget https://YOUR_ROSI_DOMAIN/downloads/install-node-exporter.sh
   chmod +x install-node-exporter.sh
   sudo ./install-node-exporter.sh
   ```

### Option B: Manual Setup

- See [RSYSLOG-SETUP.md](RSYSLOG-SETUP.md) for rsyslog configuration
- See [NODE-EXPORTER-SETUP.md](NODE-EXPORTER-SETUP.md) for node exporter installation

## Files in This Directory

| File | Description |
|------|-------------|
| `rsyslog-forward.conf` | Full-featured rsyslog forwarding configuration |
| `rsyslog-forward-minimal.conf` | Minimal forwarding config (recommended) |
| `install-rsyslog-client.sh` | Automated rsyslog client setup |
| `install-node-exporter.sh` | Automated node exporter setup |
| `RSYSLOG-SETUP.md` | Detailed rsyslog instructions |
| `NODE-EXPORTER-SETUP.md` | Detailed node exporter instructions |

## Prerequisites

- Ubuntu/Debian-based Linux system
- Root or sudo access
- Network connectivity to ROSI Collector
- ROSI Collector IP address or hostname

## Network Requirements

Ensure the following ports are accessible:

- **Outbound TCP 10514** - For rsyslog log forwarding (plaintext)
- **Outbound TCP 6514** - For rsyslog TLS forwarding (if TLS enabled on server)
- **Inbound TCP 9100** - For Prometheus metrics scraping (ROSI Collector -> client)

## TLS Client Setup (Optional)

If TLS is enabled on your ROSI Collector (port 6514), clients need certificates:

```bash
# On the ROSI Collector server, generate client certificate:
rosi-generate-client-cert --download client-hostname

# This creates a downloadable package. On the client:
wget https://YOUR_ROSI_DOMAIN/downloads/tls-packages/client-hostname.tar.gz
tar xzf client-hostname.tar.gz
sudo mv *.pem /etc/rsyslog.d/tls/
```

Then configure rsyslog to use TLS (see [RSYSLOG-SETUP.md](RSYSLOG-SETUP.md) for details).

## Firewall Configuration

On the client host, ensure firewall allows:

```bash
# Allow inbound connections for Prometheus node exporter (from ROSI Collector)
sudo ufw allow from ROSI_COLLECTOR_IP to any port 9100 proto tcp

# Verify outbound connectivity to ROSI Collector
telnet ROSI_COLLECTOR_IP 10514
```

## Verification

After setup, verify both services are working:

```bash
# Test rsyslog forwarding
logger "test message from $(hostname)"
# Check Grafana on your ROSI Collector to verify receipt

# Test node exporter
curl http://localhost:9100/metrics
# Or from ROSI Collector:
curl http://CLIENT_IP:9100/metrics
```

## Adding Client to ROSI Collector

After configuring the client, add it to the ROSI Collector's Prometheus targets:

1. SSH to the ROSI Collector server and add the target:
   ```bash
   prometheus-target add CLIENT_IP:9100 host=CLIENT_HOSTNAME role=ROLE network=NETWORK
   
   # Example:
   prometheus-target add 10.135.0.10:9100 host=webserver.example.com role=web network=internal
   ```

2. Verify the target was added:
   ```bash
   prometheus-target list
   ```

3. Prometheus picks up changes automatically within 5 minutes (no restart needed)

4. Verify in Grafana or Prometheus UI:
   - Navigate to Prometheus: `http://YOUR_ROSI_DOMAIN:9090`
   - Go to: Status > Targets
   - Verify client appears as "UP"

## Troubleshooting

### rsyslog Issues

- Check rsyslog status: `sudo systemctl status rsyslog`
- View rsyslog logs: `sudo tail -f /var/log/syslog | grep rsyslog`
- Test connectivity: `telnet ROSI_COLLECTOR_IP 10514`
- Check queue: `ls -lh /var/spool/rsyslog/`

### Node Exporter Issues

- Check service status: `sudo systemctl status node_exporter`
- Verify port is listening: `sudo netstat -tlnp | grep 9100`
- Test locally: `curl http://localhost:9100/metrics`
- Check firewall: `sudo ufw status | grep 9100`

### ROSI Collector Not Receiving Data

- Verify network connectivity between client and ROSI Collector
- Check ROSI Collector firewall allows connections
- Verify ROSI Collector services are running: `docker compose ps`
- Check ROSI Collector logs: `docker compose logs rsyslog prometheus`

