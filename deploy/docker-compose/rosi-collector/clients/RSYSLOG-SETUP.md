# rsyslog Client Configuration Guide

This guide explains how to configure rsyslog on client hosts to forward logs to the ROSI Collector server.

## Overview

The central server accepts syslog messages via TCP on port 10514. Client hosts forward logs using rsyslog's `omfwd` module with disk-assisted queuing for resilience.

## Installation Methods

Two installation methods are provided:

1. **Automated Script** (Recommended) - Fully automated installation with interactive prompts
2. **Manual Configuration** - Step-by-step manual installation

## Method 1: Automated Installation Script (Recommended)

The automated installation script handles all configuration steps automatically:

- Prompts for central server IP address and port
- Detects and asks to overwrite existing configurations
- Installs rsyslog client configuration
- Creates spool directory
- Tests configuration syntax
- Restarts rsyslog service
- Shows logs for error verification

### Quick Installation

Download the script from the central server, make it executable, and run it:

```bash
# Download the script from central server
# Replace CENTRAL_SERVER_DOMAIN with your central server's domain name or IP address
wget https://CENTRAL_SERVER_DOMAIN/downloads/install-rsyslog-client.sh

# Make it executable
chmod +x install-rsyslog-client.sh

# Run the installation script
sudo ./install-rsyslog-client.sh
```

**Alternative**: If you have the repository cloned locally, you can use the script from the repository:

```bash
# If you have the repository cloned locally
cd /path/to/rsyslog/deploy/docker-compose/rosi-collector/clients
sudo ./install-rsyslog-client.sh
```

**Note**: The recommended method is to download from the central server URL above, as it ensures you always get the latest version.

The script will:
1. Check for existing rsyslog client configuration
2. Prompt you to enter central server IP address
3. Prompt you to enter port (default: 10514)
4. Ask for confirmation to overwrite if configuration exists
5. Install and configure everything automatically
6. Install the rsyslog impstats sidecar (default enabled)
7. Restart rsyslog and show logs for verification

### Script Features

- **Interactive prompts**: User-friendly input for IP address and port
- **Overwrite protection**: Detects existing configs and asks before overwriting
- **Configuration testing**: Validates configuration syntax before applying
- **Automatic setup**: Creates spool directory and installs configuration
- **Impstats sidecar**: Installs the rsyslog exporter service by default
- **Exporter IP selection**: Prompts for the exporter bind address
- **Python venv check**: Offers to install `pythonX.Y-venv` if missing
- **Firewall rules**: Adds UFW or iptables-persistent rules on Ubuntu/Debian
- **Log verification**: Shows recent logs and checks for errors
- **Error handling**: Comprehensive error checking and reporting

### Sidecar Installer (Optional)

The client installer also sets up the rsyslog impstats exporter sidecar
by default. To skip sidecar installation:

```bash
sudo ./install-rsyslog-client.sh --no-sidecar
```

### Add Client Targets (One Command)

On the ROSI Collector server, add both node and impstats targets at once:

```bash
prometheus-target add-client CLIENT_IP host=CLIENT_HOSTNAME role=ROLE network=NETWORK
```

### Example Output

The script provides color-coded output and interactive prompts:

```
[INFO]==========================================
[INFO]rsyslog Client Configuration Installer
[INFO]==========================================

[PROMPT]Install rsyslog impstats sidecar? [Y/n]
y
[WARN]python3 venv/ensurepip module is missing.
[PROMPT]Run 'apt update && apt install python3.12-venv'? [Y/n]

[INFO]Detected IP addresses on this system:
  [1] 127.0.0.1
  [2] 10.0.3.1
[PROMPT]Select IP address to bind exporter [1-2] or enter custom IP:
2

[PROMPT]Enter central server IP address:
10.135.0.10

[PROMPT]Enter central server port [default: 10514]:
10514

[INFO]Installation configuration:
[INFO]  Target IP: 10.135.0.10
[INFO]  Target Port: 10514

[PROMPT]Proceed with installation? [y/N]
y

[INFO]Installing rsyslog client configuration...
[INFO]Configuration file created successfully
[INFO]Spool directory already exists: /var/spool/rsyslog
[INFO]Testing rsyslog configuration...
[INFO]Configuration test passed
[INFO]Restarting rsyslog service...
[INFO]rsyslog service restarted successfully
```

## Method 2: Manual Configuration

If you prefer to configure rsyslog manually, follow the steps below.

## Configuration Files

Two configuration files are provided:

1. **`rsyslog-forward-minimal.conf`** (Recommended)
   - Simple, production-ready configuration
   - Forwards all logs with disk-assisted queue
   - Best for most use cases

2. **`client-rsyslog-forward.conf`** (Advanced)
   - Full-featured with multiple forwarding options
   - Includes examples for selective forwarding
   - Best for custom filtering requirements

## Manual Installation Steps

### Step 1: Choose Configuration File

For most deployments, use the minimal configuration:

```bash
CONFIG_FILE="rsyslog-forward-minimal.conf"
```

For advanced filtering needs, use the full configuration:

```bash
CONFIG_FILE="client-rsyslog-forward.conf"
```

### Step 2: Edit Configuration

Replace `CHANGE_ME` with your central server IP address or hostname:

```bash
# Copy configuration file
cp ${CONFIG_FILE} /tmp/99-forward-to-central.conf

# Edit the file
sudo nano /tmp/99-forward-to-central.conf

# Replace "CHANGE_ME" with central server IP/hostname
# Example: target="10.135.0.10"
# Or: target="central.example.com"
```

**Important**: Keep the quotes around the IP address/hostname.

### Step 3: Create Spool Directory

The configuration uses disk-assisted queuing. Create the spool directory:

```bash
sudo mkdir -p /var/spool/rsyslog
sudo chmod 750 /var/spool/rsyslog
```

### Step 4: Install Configuration

Copy the edited configuration to rsyslog.d:

```bash
sudo cp /tmp/99-forward-to-central.conf /etc/rsyslog.d/99-forward-to-central.conf
sudo chmod 644 /etc/rsyslog.d/99-forward-to-central.conf
```

### Step 5: Verify Configuration Syntax

Test the configuration before restarting:

```bash
sudo rsyslogd -N1 -f /etc/rsyslog.conf
```

If there are no errors, the configuration is valid.

### Step 6: Restart rsyslog

Apply the configuration:

```bash
sudo systemctl restart rsyslog
```

### Step 7: Verify Service Status

Check that rsyslog is running:

```bash
sudo systemctl status rsyslog
```

You should see "active (running)".

### Step 8: Test Log Forwarding

Send a test message:

```bash
logger "test message from $(hostname) at $(date)"
```

Verify the message appears on the central server (check Grafana or central server logs).

## Configuration Details

### Minimal Configuration

The minimal configuration forwards all logs with these features:

- **Protocol**: TCP (reliable delivery)
- **Port**: 10514
- **Queue**: Disk-assisted linked list queue
- **Queue Size**: 10,000 messages in memory
- **Disk Queue**: Up to 100MB on disk
- **Resilience**: Automatically retries on connection failure
- **Persistence**: Saves queue on shutdown, resumes on startup

### Advanced Configuration Options

The full configuration file includes examples for:

- Forwarding only specific facilities (auth, authpriv)
- Forwarding only high-severity logs (error and above)
- Forwarding logs from specific applications
- Excluding certain facilities (local0, local1)

To use these options:

1. Comment out OPTION 1 (the default `*.*` rule)
2. Uncomment and customize the desired OPTION 2 rule
3. Replace `CHANGE_ME` in the selected rule
4. Restart rsyslog

## Queue Management

The configuration uses disk-assisted queuing for resilience:

- **In-Memory Queue**: 10,000 messages (fast)
- **Disk Queue**: Up to 100MB (persistent)
- **File Size Limit**: 10MB per queue file
- **Location**: `/var/spool/rsyslog/forward-queue*`

When the central server is unavailable:

1. Logs are queued in memory first
2. When memory queue fills, logs spill to disk
3. When connection is restored, queued logs are forwarded automatically
4. Oldest logs are forwarded first (FIFO)

### Monitoring Queue Status

Check queue files:

```bash
# List queue files
ls -lh /var/spool/rsyslog/forward-queue*

# Check queue size
du -sh /var/spool/rsyslog/
```

If queue files are growing, the central server may be unreachable.

## Firewall Configuration

Ensure outbound TCP connections to port 10514 are allowed:

```bash
# Test connectivity
telnet CENTRAL_SERVER_IP 10514

# If using UFW, allow outbound (usually allowed by default)
sudo ufw status
```

## Troubleshooting

### rsyslog Not Starting

Check configuration syntax:

```bash
sudo rsyslogd -N1
```

View error messages:

```bash
sudo journalctl -u rsyslog -n 50
```

### Logs Not Appearing on Central Server

1. **Test connectivity**:
   ```bash
   telnet CENTRAL_SERVER_IP 10514
   ```

2. **Check rsyslog status**:
   ```bash
   sudo systemctl status rsyslog
   ```

3. **View rsyslog logs**:
   ```bash
   sudo tail -f /var/log/syslog | grep rsyslog
   ```

4. **Check for errors**:
   ```bash
   sudo journalctl -u rsyslog --since "10 minutes ago" | grep -i error
   ```

5. **Verify configuration is loaded**:
   ```bash
   sudo rsyslogd -N1 -f /etc/rsyslog.conf
   ```

6. **Test message forwarding**:
   ```bash
   logger "test message"
   # Check central server immediately after
   ```

### Queue Files Growing

If queue files are growing, the central server may be unreachable:

1. **Check connectivity**:
   ```bash
   telnet CENTRAL_SERVER_IP 10514
   ```

2. **Check central server status**:
   - Verify central server rsyslog container is running
   - Check central server firewall allows port 10514

3. **Monitor queue**:
   ```bash
   watch -n 5 'ls -lh /var/spool/rsyslog/forward-queue*'
   ```

When connectivity is restored, queued logs will be forwarded automatically.

### High Disk Usage

If queue directory is using too much disk space:

1. **Check current usage**:
   ```bash
   du -sh /var/spool/rsyslog/
   ```

2. **Review queue files**:
   ```bash
   ls -lh /var/spool/rsyslog/forward-queue*
   ```

3. **If needed, increase maxDiskSpace** in configuration:
   ```bash
   queue.maxDiskSpace="500m"  # Increase from 100m to 500m
   ```

4. **Restart rsyslog**:
   ```bash
   sudo systemctl restart rsyslog
   ```

**Note**: Increasing queue size is a temporary solution. Address the root cause (connectivity issues) to prevent queue growth.

## Verification Checklist

- [ ] Configuration file edited with correct central server IP
- [ ] Spool directory created (`/var/spool/rsyslog`)
- [ ] Configuration syntax verified (`rsyslogd -N1`)
- [ ] Configuration installed to `/etc/rsyslog.d/`
- [ ] rsyslog restarted successfully
- [ ] Service status shows "active (running)"
- [ ] Test message sent and verified on central server
- [ ] Connectivity to central server confirmed (telnet)
- [ ] Queue files created (if connection was temporarily unavailable)

## Maintenance

### Regular Checks

- Monitor queue directory size: `du -sh /var/spool/rsyslog/`
- Check rsyslog status: `sudo systemctl status rsyslog`
- Review rsyslog logs: `sudo journalctl -u rsyslog --since "1 day ago"`

### Log Rotation

rsyslog handles log rotation automatically. The queue files are managed by rsyslog and don't require manual rotation.

### Updating Configuration

To update the configuration:

1. Edit the file in `/etc/rsyslog.d/99-forward-to-central.conf`
2. Verify syntax: `sudo rsyslogd -N1`
3. Restart: `sudo systemctl restart rsyslog`
4. Verify: `sudo systemctl status rsyslog`

## Security Considerations

- The configuration forwards all system logs to the central server
- Ensure network connectivity is secured (VPN, firewall rules)
- Queue files may contain sensitive log data - ensure proper file permissions
- **Recommended**: Use TLS for encrypted log forwarding (see TLS Configuration below)

## TLS Configuration (Encrypted Log Forwarding)

For encrypted syslog transport, configure the client to use TLS on port 6514.

### Prerequisites

1. TLS must be enabled on the ROSI Collector server (`SYSLOG_TLS_ENABLED=true`)
2. Client certificates must be generated (for mTLS modes)

### Step 1: Obtain Certificates

**Option A: Download from ROSI Collector (Recommended)**

On the ROSI Collector server:
```bash
# Generate client certificate package
rosi-generate-client-cert --download $(hostname -f)
```

On the client, download and install:
```bash
# Download CA certificate (always required)
wget https://YOUR_ROSI_DOMAIN/downloads/ca.pem

# For mTLS, download your client certificate package
wget https://YOUR_ROSI_DOMAIN/downloads/tls-packages/CLIENT_NAME.tar.gz
tar xzf CLIENT_NAME.tar.gz

# Install certificates
sudo mkdir -p /etc/rsyslog.d/tls
sudo mv ca.pem client-cert.pem client-key.pem /etc/rsyslog.d/tls/
sudo chmod 600 /etc/rsyslog.d/tls/*.pem
sudo chown root:root /etc/rsyslog.d/tls/*.pem
```

### Step 2: Configure rsyslog for TLS

Create `/etc/rsyslog.d/99-forward-tls.conf`:

**For TLS without client certificates (anon mode):**
```bash
# Load TLS driver
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.pem"
)

# Forward all logs via TLS
*.* action(
    type="omfwd"
    target="YOUR_ROSI_COLLECTOR_IP"
    port="6514"
    protocol="tcp"
    streamDriver="gtls"
    streamDriverMode="1"
    streamDriverAuthMode="anon"
    queue.type="LinkedList"
    queue.filename="fwd_tls"
    queue.maxdiskspace="500m"
    queue.saveonshutdown="on"
    action.resumeRetryCount="-1"
)
```

**For mTLS (x509/certvalid or x509/name mode):**
```bash
# Load TLS driver with client certificate
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.pem"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/client-cert.pem"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/client-key.pem"
)

# Forward all logs via mTLS
*.* action(
    type="omfwd"
    target="YOUR_ROSI_COLLECTOR_IP"
    port="6514"
    protocol="tcp"
    streamDriver="gtls"
    streamDriverMode="1"
    streamDriverAuthMode="x509/name"
    streamDriverPermittedPeers="YOUR_ROSI_DOMAIN"
    queue.type="LinkedList"
    queue.filename="fwd_tls"
    queue.maxdiskspace="500m"
    queue.saveonshutdown="on"
    action.resumeRetryCount="-1"
)
```

### Step 3: Test and Apply

```bash
# Test configuration
sudo rsyslogd -N1

# Restart rsyslog
sudo systemctl restart rsyslog

# Verify connection
sudo tail -f /var/log/syslog | grep -i tls
```

### TLS Troubleshooting

| Issue | Solution |
|-------|----------|
| "certificate verify failed" | Check CA certificate matches server's CA |
| "connection refused" | Verify port 6514 is open, TLS enabled on server |
| "peer name mismatch" | Ensure `streamDriverPermittedPeers` matches server cert |
| "no certificate" | For mTLS, ensure client cert/key files exist |

## Additional Resources

- rsyslog documentation: https://www.rsyslog.com/doc/
- omfwd module: https://www.rsyslog.com/doc/v8-stable/configuration/modules/omfwd.html
- Queue configuration: https://www.rsyslog.com/doc/v8-stable/configuration/queues.html

