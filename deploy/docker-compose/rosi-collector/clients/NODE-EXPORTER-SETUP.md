# Prometheus Node Exporter Setup Guide

This guide explains how to install and configure Prometheus Node Exporter on client hosts for metrics collection by the ROSI Collector server.

## Overview

Prometheus Node Exporter exposes system metrics (CPU, memory, disk, network) that the central server's Prometheus instance scrapes. By default, node exporter binds to all interfaces (0.0.0.0), but for security, we'll configure it to bind only to the local network interface (e.g., 10.x.x.x).

## Installation Methods

Three installation methods are provided:

1. **Automated Script** (Recommended) - Fully automated installation with latest version detection
2. **Manual Systemd Service** - Step-by-step manual installation
3. **Docker Container** - Alternative for containerized environments

## Prerequisites

- Ubuntu/Debian-based Linux system
- Root or sudo access
- Network connectivity to central server
- Local network IP address (e.g., 10.x.x.x) - determine your interface IP

## Determine Local Network IP

Before installation, determine your local network IP address:

```bash
# Option 1: Using ip command
ip addr show | grep -E "inet.*10\." | awk '{print $2}' | cut -d/ -f1

# Option 2: Using hostname
hostname -I | awk '{for(i=1;i<=NF;i++) if($i ~ /^10\./) print $i}'

# Option 3: List all IPs and identify local network
ip addr show

# Option 4: Check specific interface (e.g., eth0, ens3)
ip addr show eth0 | grep "inet " | awk '{print $2}' | cut -d/ -f1
```

**Note**: If your network uses a different range (e.g., 192.168.x.x, 172.16.x.x), adjust the grep pattern accordingly.

## Method 1: Automated Installation Script (Recommended)

The automated installation script handles all steps automatically:

- Fetches latest version from GitHub
- Detects local network IP addresses
- Prompts for IP selection
- Installs and configures node_exporter
- Creates systemd service
- Configures firewall rules
- Verifies installation

### Quick Installation

Download the script from the central server, make it executable, and run it:

```bash
# Download the script from central server
# Replace CENTRAL_SERVER_DOMAIN with your central server's domain name or IP address
wget https://CENTRAL_SERVER_DOMAIN/downloads/install-node-exporter.sh

# Make it executable
chmod +x install-node-exporter.sh

# Run the installation script
sudo ./install-node-exporter.sh
```

**Alternative**: If you have the repository cloned locally, you can use the script from the repository:

```bash
# If you have the repository cloned locally
cd /path/to/rsyslog/deploy/docker-compose/rosi-collector/clients
sudo ./install-node-exporter.sh
```

**Note**: The recommended method is to download from the central server URL above, as it ensures you always get the latest version.

The script will:
1. Automatically detect the latest node_exporter version
2. Show detected local network IP addresses
3. Prompt you to select which IP to use
4. Optionally prompt for central server IP for firewall configuration
5. Install and configure everything automatically

### Script Features

- **Automatic version detection**: Always installs the latest version
- **IP detection**: Automatically detects local network IPs (10.x.x.x, 192.168.x.x, 172.16-31.x.x)
- **Interactive prompts**: User-friendly selection of IP addresses
- **Firewall configuration**: Optional automatic UFW rule creation
- **Verification**: Automatically verifies installation success
- **Error handling**: Comprehensive error checking and reporting

### Example Output

```
[INFO] ==========================================
[INFO] Prometheus Node Exporter Installer
[INFO] ==========================================

[INFO] Fetching latest node_exporter version from GitHub...
[INFO] Latest version: 1.7.0
[INFO] Detected local network IP addresses:
  [1] 10.135.0.10
  [2] 10.135.0.20
[PROMPT] Select IP address to bind node_exporter [1-2] (default: 1):
1
[INFO] Selected IP: 10.135.0.10
[PROMPT] Enter central server IP address for firewall rule (optional, press Enter to skip):
10.135.0.1

[INFO] Installation configuration:
  Version: 1.7.0
  Architecture: amd64
  Bind IP: 10.135.0.10
  Central Server IP: 10.135.0.1

[PROMPT] Proceed with installation? [y/N]
y

[INFO] Installing node_exporter version 1.7.0 for architecture amd64...
...
[INFO] Installation completed successfully!
```

## Method 2: Manual Systemd Service Installation

### Step 1: Download Node Exporter

Download the latest release (check https://github.com/prometheus/node_exporter/releases for latest version):

```bash
# Set version (update to latest)
NODE_EXPORTER_VERSION="1.7.0"

# Download
cd /tmp
wget https://github.com/prometheus/node_exporter/releases/download/v${NODE_EXPORTER_VERSION}/node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64.tar.gz

# Extract
tar xvf node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64.tar.gz
cd node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64
```

### Step 2: Install Binary

Copy binary to system directory:

```bash
sudo cp node_exporter /usr/local/bin/
sudo chmod +x /usr/local/bin/node_exporter
```

### Step 3: Create System User

Create a dedicated user for node exporter:

```bash
sudo useradd --no-create-home --shell /bin/false node_exporter
```

### Step 4: Determine Local Network IP

Set your local network IP address:

```bash
# Replace with your actual local network IP
LOCAL_IP="10.135.0.10"

# Or auto-detect (first 10.x.x.x IP found)
LOCAL_IP=$(ip addr show | grep -E "inet.*10\." | head -1 | awk '{print $2}' | cut -d/ -f1)

# Verify
echo "Local IP: $LOCAL_IP"
```

**Important**: Ensure this IP is reachable from the central server.

### Step 5: Create Systemd Service File

Create systemd service with local network binding:

```bash
sudo tee /etc/systemd/system/node_exporter.service > /dev/null <<EOF
[Unit]
Description=Prometheus Node Exporter
Documentation=https://github.com/prometheus/node_exporter
After=network.target

[Service]
User=node_exporter
Group=node_exporter
Type=simple
ExecStart=/usr/local/bin/node_exporter \\
    --web.listen-address=${LOCAL_IP}:9100 \\
    --path.procfs=/proc \\
    --path.sysfs=/sys \\
    --collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)(\$|/)
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
```

**Note**: Replace `${LOCAL_IP}` with your actual IP, or use the variable if set in your shell.

### Step 6: Reload Systemd and Start Service

```bash
# Reload systemd to recognize new service
sudo systemctl daemon-reload

# Enable service to start on boot
sudo systemctl enable node_exporter

# Start service
sudo systemctl start node_exporter
```

### Step 7: Verify Installation

Check service status:

```bash
sudo systemctl status node_exporter
```

Verify it's listening on the correct interface:

```bash
# Should show node_exporter listening on LOCAL_IP:9100
sudo netstat -tlnp | grep 9100
# Or using ss:
sudo ss -tlnp | grep 9100
```

Test metrics endpoint:

```bash
# From localhost
curl http://${LOCAL_IP}:9100/metrics | head -20

# From central server (if accessible)
curl http://${LOCAL_IP}:9100/metrics | head -20
```

### Step 8: Configure Firewall

Allow inbound connections from central server:

```bash
# Replace CENTRAL_SERVER_IP with actual central server IP
CENTRAL_SERVER_IP="10.135.0.1"

# Allow from central server only
sudo ufw allow from ${CENTRAL_SERVER_IP} to any port 9100 proto tcp comment "Prometheus node exporter"

# Verify rule
sudo ufw status | grep 9100
```

## Method 3: Docker Container Installation

If you prefer Docker, use this method:

### Step 1: Determine Local Network IP

```bash
LOCAL_IP=$(ip addr show | grep -E "inet.*10\." | head -1 | awk '{print $2}' | cut -d/ -f1)
echo "Local IP: $LOCAL_IP"
```

### Step 2: Create Docker Compose File

```bash
sudo mkdir -p /opt/node-exporter
cd /opt/node-exporter

sudo tee docker-compose.yml > /dev/null <<EOF
version: '3.8'

services:
  node_exporter:
    image: prom/node-exporter:latest
    container_name: node_exporter
    restart: unless-stopped
    network_mode: host
    command:
      - '--path.procfs=/host/proc'
      - '--path.sysfs=/host/sys'
      - '--collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)(\$|/)'
      - '--web.listen-address=${LOCAL_IP}:9100'
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /:/rootfs:ro
    pid: host
EOF
```

**Note**: Replace `${LOCAL_IP}` with your actual IP address.

### Step 3: Start Container

```bash
cd /opt/node-exporter
sudo docker compose up -d
```

### Step 4: Verify

```bash
# Check container status
sudo docker ps | grep node_exporter

# Check listening port
sudo netstat -tlnp | grep 9100

# Test metrics
curl http://${LOCAL_IP}:9100/metrics | head -20
```

### Step 5: Create Systemd Service (Optional)

To manage Docker container via systemd:

```bash
sudo tee /etc/systemd/system/node-exporter-docker.service > /dev/null <<EOF
[Unit]
Description=Prometheus Node Exporter (Docker)
Requires=docker.service
After=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=/opt/node-exporter
ExecStart=/usr/bin/docker compose up -d
ExecStop=/usr/bin/docker compose down
TimeoutStartSec=0

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable node-exporter-docker.service
```

## Configuration Options

### Binding to Specific Interface

The `--web.listen-address` flag controls which interface node exporter binds to:

- **All interfaces** (default): `0.0.0.0:9100` - accessible from any network
- **Local network only**: `10.135.0.10:9100` - accessible only from local network
- **Localhost only**: `127.0.0.1:9100` - accessible only from localhost (not recommended for remote scraping)

**Best Practice**: Bind to local network IP (e.g., `10.x.x.x:9100`) and use firewall rules to restrict access to central server only.

### Collector Options

Node exporter includes many collectors. Common options:

```bash
# Disable specific collectors
--no-collector.arp
--no-collector.bcache

# Enable additional collectors
--collector.systemd
--collector.textfile.directory=/var/lib/node_exporter/textfile_collector
```

See full list: https://github.com/prometheus/node_exporter#collectors

## Firewall Configuration

### UFW (Uncomplicated Firewall)

```bash
# Allow from central server only
sudo ufw allow from CENTRAL_SERVER_IP to any port 9100 proto tcp comment "Prometheus node exporter"

# Or allow from entire subnet (less secure)
sudo ufw allow from 10.135.0.0/24 to any port 9100 proto tcp comment "Prometheus node exporter"
```

### iptables (Advanced)

```bash
# Allow from central server only
sudo iptables -A INPUT -p tcp -s CENTRAL_SERVER_IP --dport 9100 -j ACCEPT

# Save rules (Ubuntu/Debian)
sudo netfilter-persistent save
```

## Adding Client to Central Server

After installation, add the client to the central server's Prometheus targets using the `prometheus-target` helper script:

1. **SSH to the central server** and run:
   ```bash
   # Add new target with labels
   prometheus-target add CLIENT_IP:9100 host=CLIENT_HOSTNAME role=ROLE network=NETWORK
   
   # Example:
   prometheus-target add 10.135.0.10:9100 host=webserver.example.com role=web network=internal
   ```

2. **Verify the target was added**:
   ```bash
   prometheus-target list
   ```

3. **Wait for Prometheus to pick up changes** (within 5 minutes, no restart needed)

4. **Verify in Prometheus UI**:
   - Navigate to: http://CENTRAL_SERVER_IP:9090
   - Go to: Status > Targets
   - Verify client appears with status "UP"

**Supported labels**:
- `host` - Hostname (required)
- `role` - Server role (e.g., web, db, monitoring, docker-host)
- `network` - Network type (internal, external)
- `env` - Environment (production, staging, dev)

## Troubleshooting

### Service Not Starting

Check service status and logs:

```bash
# Systemd service
sudo systemctl status node_exporter
sudo journalctl -u node_exporter -n 50

# Docker container
sudo docker ps -a | grep node_exporter
sudo docker logs node_exporter
```

### Port Not Listening

Verify node exporter is listening:

```bash
# Check if port is listening
sudo netstat -tlnp | grep 9100
sudo ss -tlnp | grep 9100

# Check if binding to correct IP
sudo lsof -i :9100
```

If not listening, check:

1. Service is running: `sudo systemctl status node_exporter`
2. Correct IP in service file: `sudo cat /etc/systemd/system/node_exporter.service`
3. Firewall blocking: `sudo ufw status`

### Cannot Access from Central Server

1. **Test connectivity**:
   ```bash
   # From central server
   telnet CLIENT_IP 9100
   curl http://CLIENT_IP:9100/metrics
   ```

2. **Check firewall rules**:
   ```bash
   # On client
   sudo ufw status | grep 9100
   sudo iptables -L -n | grep 9100
   ```

3. **Verify IP binding**:
   ```bash
   # On client
   sudo netstat -tlnp | grep 9100
   # Should show CLIENT_IP:9100, not 0.0.0.0:9100
   ```

4. **Check network routing**:
   ```bash
   # From central server
   ping CLIENT_IP
   traceroute CLIENT_IP
   ```

### Metrics Not Appearing in Prometheus

1. **Verify target is configured**:
   ```bash
   # On central server
   prometheus-target list
   ```

2. **Check Prometheus targets**:
   - Navigate to: http://CENTRAL_SERVER_IP:9090
   - Go to: Status > Targets
   - Check if client shows as "UP" or "DOWN"

3. **Check Prometheus logs**:
   ```bash
   cd ${INSTALL_DIR:-/opt/rosi-collector}
   docker compose logs prometheus | grep CLIENT_IP
   ```

4. **Verify target file**:
   ```bash
   cat ${INSTALL_DIR:-/opt/rosi-collector}/prometheus-targets/nodes.yml
   ```

### High Resource Usage

Node exporter is lightweight, but if experiencing issues:

1. **Check collector usage**:
   ```bash
   curl http://CLIENT_IP:9100/metrics | grep -c "^#"
   ```

2. **Disable unnecessary collectors** in service file:
   ```bash
   --no-collector.arp
   --no-collector.bcache
   ```

3. **Monitor node exporter process**:
   ```bash
   top -p $(pgrep node_exporter)
   ```

## Security Best Practices

1. **Bind to Local Network IP**: Use `--web.listen-address=LOCAL_IP:9100` instead of `0.0.0.0:9100`

2. **Firewall Rules**: Restrict access to central server only:
   ```bash
   sudo ufw allow from CENTRAL_SERVER_IP to any port 9100 proto tcp
   ```

3. **Run as Non-Root**: Node exporter runs as dedicated user (`node_exporter`)

4. **Network Segmentation**: Use VPN or private network for metrics collection

5. **Regular Updates**: Keep node exporter updated to latest version

6. **Monitor Access**: Review firewall logs for unauthorized access attempts

## Verification Checklist

- [ ] Local network IP determined and verified
- [ ] Node exporter binary installed or Docker image pulled
- [ ] Systemd service created with correct IP binding
- [ ] Service enabled and started
- [ ] Service status shows "active (running)"
- [ ] Port 9100 listening on correct interface
- [ ] Metrics endpoint accessible locally
- [ ] Firewall rules configured (central server access only)
- [ ] Metrics accessible from central server
- [ ] Client added to central server using `prometheus-target add`
- [ ] Prometheus shows target as "UP"

## Maintenance

### Updating Node Exporter

**Systemd Method**:

```bash
# Stop service
sudo systemctl stop node_exporter

# Download new version
NODE_EXPORTER_VERSION="1.7.0"  # Update version
cd /tmp
wget https://github.com/prometheus/node_exporter/releases/download/v${NODE_EXPORTER_VERSION}/node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64.tar.gz
tar xvf node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64.tar.gz

# Replace binary
sudo cp node_exporter-${NODE_EXPORTER_VERSION}.linux-amd64/node_exporter /usr/local/bin/
sudo chmod +x /usr/local/bin/node_exporter

# Start service
sudo systemctl start node_exporter
```

**Docker Method**:

```bash
cd /opt/node-exporter
sudo docker compose pull
sudo docker compose up -d
```

### Monitoring Service Health

```bash
# Check service status
sudo systemctl status node_exporter

# View recent logs
sudo journalctl -u node_exporter --since "1 hour ago"

# Check metrics endpoint
curl -s http://LOCAL_IP:9100/metrics | grep node_exporter_build_info
```

## Additional Resources

- Node Exporter GitHub: https://github.com/prometheus/node_exporter
- Prometheus Documentation: https://prometheus.io/docs/
- Node Exporter Collectors: https://github.com/prometheus/node_exporter#collectors

