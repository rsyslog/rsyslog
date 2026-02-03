# ROSI Collector

**Rsyslog Operations Stack Initiative** - A production-ready centralized log collection and monitoring stack.

ROSI Collector provides a complete solution for collecting, storing, and visualizing system logs from multiple hosts using rsyslog, Loki, Grafana, and Prometheus.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       ROSI Collector                            │
│  ┌─────────────┐  ┌─────────┐  ┌──────────┐  ┌──────────────┐  │
│  │   Traefik   │  │  Loki   │  │ Grafana  │  │  Prometheus  │  │
│  │   (proxy)   │  │ (logs)  │  │  (viz)   │  │  (metrics)   │  │
│  └──────┬──────┘  └────┬────┘  └─────┬────┘  └──────┬───────┘  │
│         │              ▲             │              │          │
│         │              │             │              │          │
│  ┌──────┴──────────────┴─────────────┴──────────────┴───────┐  │
│  │                    rsyslog (omhttp)                       │  │
│  │             (log receiver, TCP 10514/6514)                │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
         ▲                                        ▲
         │ logs (TCP 10514/6514)                  │ metrics (9100)
         │                                        │
┌────────┴────────┐                      ┌────────┴────────┐
│   Client Host   │                      │   Client Host   │
│  (rsyslog +     │  ...                 │  (rsyslog +     │
│  node-exporter) │                      │  node-exporter) │
└─────────────────┘                      └─────────────────┘

         ▲ Server also forwards its own logs to localhost:10514
         │ (configured automatically by init.sh)
```

## Quick Start

### Prerequisites

- Linux server (Ubuntu 24.04 LTS recommended, tested)
- Docker Engine 20.10+
- Docker Compose v2
- 2GB+ RAM (4GB+ recommended)
- Domain name (for TLS) or local deployment

> **Note**: The installation scripts have been tested on Ubuntu 24.04 LTS.
> Other Debian-based distributions should work with minor adjustments.

### 1. Clone Repository

```bash
git clone https://github.com/rsyslog/rsyslog.git
cd rsyslog/deploy/docker-compose/rosi-collector
```

### 2. Initialize Environment

Run the initialization script (as root):

```bash
sudo ./scripts/init.sh
```

The script will interactively prompt for:
- **Installation directory** (default: `/opt/rosi-collector`)
- **TRAEFIK_DOMAIN** - Domain or IP for accessing Grafana (required)
- **TRAEFIK_EMAIL** - Email for Let's Encrypt certificates
- **GRAFANA_ADMIN_PASSWORD** - Leave empty to auto-generate
- **TLS Configuration** - Enable encrypted syslog on port 6514
- **Server syslog forwarding** - Forward server's own logs to collector

For non-interactive installation, set environment variables:

```bash
sudo TRAEFIK_DOMAIN=logs.example.com \
     TRAEFIK_EMAIL=admin@example.com \
     ./scripts/init.sh
```

For non-interactive server self-monitoring configuration:

```bash
sudo TRAEFIK_DOMAIN=logs.example.com \
     TRAEFIK_EMAIL=admin@example.com \
     SERVER_SYSLOG_FORWARDING=true \
     ./scripts/init.sh
```

The script will:
- Copy all configuration files
- Generate `.env` with secure passwords
- Install and configure node_exporter for server self-monitoring
- Add server to Prometheus targets (using `prometheus-target` helper)
- Configure server syslog forwarding to collector (optional)
- Create Docker network and systemd service
- Generate TLS certificates (if TLS enabled)
### 3. Start the Stack

```bash
cd /opt/rosi-collector  # or your chosen install directory
docker compose up -d

# Check status
docker compose ps

# View logs
docker compose logs -f
```

### 4. Access Grafana

- Open `https://logs.example.com` (your configured TRAEFIK_DOMAIN)
- Login with `admin` / password from `.env` file:
  ```bash
  grep GRAFANA_ADMIN_PASSWORD /opt/rosi-collector/.env
  ```
- Pre-configured dashboards are available under **Dashboards**. To change dashboard layout or panels, edit the JSON files in `grafana/provisioning/dashboards/templates/` and run `python3 scripts/render-dashboards.py` to update `generated/`; then copy to the install directory and restart Grafana.

### 5. Configure Clients

On each client host, forward logs to your ROSI Collector:

```bash
# Download and run client setup script
wget https://your-domain.com/downloads/install-rsyslog-client.sh
sudo bash install-rsyslog-client.sh
```

Note: `omfwd` is built-in in rsyslog and does not require `module(load="omfwd")`.
If you see errors about `omfwd.so` missing, remove any explicit module load.

See `clients/README.md` for detailed client setup instructions.

## Services

| Service | Port | Description |
|---------|------|-------------|
| Traefik | 80, 443 | Reverse proxy with automatic TLS |
| Grafana | 3000 | Log visualization and dashboards |
| Loki | 3100 | Log storage and querying |
| Prometheus | 9090 | Metrics collection |
| rsyslog | 10514 | Log receiver (TCP plaintext) |
| rsyslog | 6514 | Log receiver (TCP TLS, if enabled) |
| node_exporter | 9100 | Host metrics (auto-installed and configured on server) |
| rsyslog-impstats exporter | 9898 | rsyslog impstats sidecar metrics (optional) |

## Directory Structure

```
rosi-collector/
├── docker-compose.yml          # Main stack definition
├── .env.template               # Environment template
├── loki-config.yml             # Loki configuration
├── prometheus.yml              # Prometheus configuration
├── logrotate.conf              # Log rotation
├── rsyslog.conf/               # rsyslog configuration
├── traefik/                    # Traefik configuration
├── prometheus-targets/         # Prometheus scrape targets
│   ├── nodes.yml               # node_exporter targets
│   └── impstats.yml            # rsyslog impstats sidecar targets
├── grafana/
│   └── provisioning/
│       ├── dashboards/
│       │   ├── dashboards.yml  # Points to generated/
│       │   ├── templates/      # Edit these; run render-dashboards.py
│       │   └── generated/      # Provisioned JSON (do not edit by hand)
│       ├── datasources/        # Loki & Prometheus configs
│       └── alerting/           # Alert rules (default.yml)
├── scripts/
│   ├── init.sh                 # Initialize environment
│   ├── monitor.sh              # Health monitoring (rosi-monitor)
│   ├── prometheus-target.sh    # Manage scrape targets (node + impstats)
│   ├── render-dashboards.py    # Build generated/ from templates/
│   ├── generate-ca.sh          # Generate TLS CA and server certs
│   ├── generate-client-cert.sh # Generate client certificates
│   ├── install-server.sh       # Prepare fresh Ubuntu server
│   └── configs/                # Server config templates
└── clients/                    # Client setup resources
```

## Pre-built Dashboards

ROSI Collector includes several pre-configured Grafana dashboards (provisioned from `grafana/provisioning/dashboards/generated/`):

| Dashboard | Description |
|-----------|-------------|
| Syslog Explorer | Search and browse logs by host, time range, and filters |
| Syslog Analysis | Distribution analysis (severity, hosts, facilities) |
| Syslog Health | rsyslog impstats metrics (queues, actions, input, output, CPU) — see below |
| Host Metrics Overview | Node exporter metrics (CPU, memory, disk) per host |
| Alerting Overview | Active alerts and alert rule status |

### Syslog Health (impstats) dashboard

When client hosts run the **impstats sidecar** (default with `install-rsyslog-client.sh`), Prometheus scrapes the sidecar’s `/metrics` endpoint. The **Syslog Health** dashboard visualizes those metrics. Add impstats targets with:

```bash
prometheus-target --job impstats add <client-ip>:9898 host=<hostname> role=rsyslog network=internal
# Or add both node and impstats for a client in one step:
prometheus-target add-client <client-ip> host=<hostname> role=rsyslog network=internal
```

The dashboard is grouped into sections:

| Section | Contents |
|---------|----------|
| **Overview** | Exporter count, action failures, suspended actions, queue full, open files, max RSS |
| **Queues** | Queue size vs max, utilization %, drop rates (discarded_full / discarded_nf) |
| **Input** | Ingest rate by input (e.g. imuxsock) |
| **Actions** | Action processed rate, action failures by action, suspended duration (collapsed by default) |
| **Output & resource usage** | Output bytes (omfwd) and CPU usage (impstats) side by side |

Use the **Host** dropdown to filter by client. For metric details and suggested alert thresholds, see `ROSI_IMPSTATS_PLAN.md`.

## Management Scripts

### Initialize Environment

```bash
# Run from rosi-collector directory
sudo ./scripts/init.sh

# With custom install directory (one-time)
sudo INSTALL_DIR=/srv/rosi ./scripts/init.sh
```

**Configuration Persistence**: On first run, the script will prompt for a custom
install directory. Your choice is saved to `~/.config/rsyslog/rosi-collector.conf`
and automatically used for future runs.

Config file locations (in priority order):
1. Environment variable: `INSTALL_DIR=/path ./scripts/init.sh`
2. User config: `~/.config/rsyslog/rosi-collector.conf`
3. System config: `/etc/rsyslog/rosi-collector.conf`
4. Default: `/opt/rosi-collector`

**Impstats exporter firewall rule:**
If UFW/firewalld is active, `init.sh` will add a rule to allow the Docker
subnet to reach the rsyslog impstats exporter on port 9898. To disable this:
`ENABLE_IMPSTATS_FIREWALL=false ./scripts/init.sh`.

### Prometheus Targets

Use the `prometheus-target` helper to manage scrape targets. The default job
is `node` (node_exporter). For the rsyslog impstats sidecar exporter, use the
`impstats` job:

```bash
# Impstats only (sidecar on port 9898)
sudo prometheus-target --job impstats add 127.0.0.1:9898 host=rosi-collector role=rsyslog network=internal
sudo prometheus-target --job impstats list

# Add both node_exporter (9100) and impstats (9898) for a client in one step
sudo prometheus-target add-client 10.0.0.5 host=client01 role=rsyslog network=internal
```

### Prepare Fresh Server (Optional)

⚠️ **WARNING**: Only run this on fresh/new systems. Do NOT run on servers you maintain yourself - it modifies system configuration, firewall rules, and installs packages.

For fresh Ubuntu 24.04 systems, this script installs Docker, configures firewall, and prepares the server:

```bash
# Interactive installation (asks about each config file)
sudo ./scripts/install-server.sh

# Non-interactive (install all configs)
sudo NONINTERACTIVE=1 ./scripts/install-server.sh

# With custom settings
sudo SSH_PORT=22 \
     OPEN_TCP_PORTS="80 443 10514" \
     HOSTNAME_FQDN="rosi.example.com" \
     ./scripts/install-server.sh
```

The script interactively asks before installing each configuration file:
- `sysctl-hardening.conf` - Kernel networking and security settings
- `rsyslog-docker.conf` - Docker container log routing
- `docker-logs-logrotate` - Log rotation for Docker logs
- `fail2ban-custom.local` - SSH brute-force protection
- `docker-daemon.json` - Docker daemon configuration
- `docker-override.conf` - Docker systemd service overrides

### Monitor Stack Health

After running `init.sh`, the `rosi-monitor` command is available system-wide:

```bash
# Show container status (includes Docker internal IPs)
rosi-monitor status

# Show recent logs
rosi-monitor logs

# Health check
rosi-monitor health

# Check SMTP configuration
rosi-monitor smtp

# Backup configuration and data
rosi-monitor backup [name]

# Restore from backup
rosi-monitor restore [name]

# List backups
rosi-monitor backups

# Reset Grafana admin password (reads from .env)
rosi-monitor reset-password

# Reset to custom password
rosi-monitor reset-password mynewpassword

# Interactive debug menu
rosi-monitor debug

# Shell into container
rosi-monitor shell grafana
```

The `status` command displays:
- Docker Compose container status
- Individual container health
- **Docker network information** (network name, subnet, gateway)
- **Internal container IPs** (useful for debugging connectivity)
- Resource usage (CPU, memory, network I/O)

Alternatively, run from the installation directory:

```bash
cd /opt/rosi-collector
./scripts/monitor.sh status
```

### Manage Prometheus Targets

After running `init.sh`, the `prometheus-target` command is available system-wide to manage node_exporter scrape targets:

```bash
# Add a target (host label is required)
prometheus-target add 10.0.0.5:9100 host=webserver

# Add with multiple labels (role, network, env)
prometheus-target add 10.0.0.5:9100 host=webserver role=web network=internal env=production

# List all configured targets
prometheus-target list

# Remove a target by IP:port
prometheus-target remove 10.0.0.5:9100

# Remove a target by hostname
prometheus-target remove webserver
```

**Available labels:**
| Label | Description |
|-------|-------------|
| `host=<name>` | Hostname (required) |
| `role=<value>` | Server role (e.g., web, db, app) |
| `env=<value>` | Environment (e.g., production, staging) |
| `network=<value>` | Network zone (e.g., internal, dmz) |
| `<key>=<value>` | Any custom label |

Changes are picked up by Prometheus automatically within 5 minutes.

**Installation directory auto-detection:**

The script automatically detects your ROSI Collector installation by checking:
1. `INSTALL_DIR` environment variable
2. User config (`~/.config/rsyslog/rosi-collector.conf`)
3. System config (`/etc/rsyslog/rosi-collector.conf`)
4. Common locations (`/opt/rosi-collector`, `/srv/central`, `/srv/rosi-collector`)

To override, set the `INSTALL_DIR` environment variable:
```bash
INSTALL_DIR=/custom/path prometheus-target list
```

## Configuration

### Environment Variables

Core configuration (prompted interactively or set via environment):

| Variable | Default | Description |
|----------|---------|-------------|
| `TRAEFIK_DOMAIN` | - | Domain or IP for the stack (required) |
| `TRAEFIK_EMAIL` | admin@domain | Let's Encrypt email |
| `GRAFANA_ADMIN_PASSWORD` | (generated) | Grafana admin password |
| `INSTALL_DIR` | `/opt/rosi-collector` | Installation directory |

Server self-monitoring (non-interactive mode):

| Variable | Default | Description |
|----------|---------|-------------|
| `SERVER_SYSLOG_FORWARDING` | `false` | Forward server's syslog to collector |

Note: node_exporter is always installed and configured on the server. The script will:
- Install the binary if not present
- Create/configure the systemd service if missing or not running
- **Bind to the Docker bridge gateway IP** (so Prometheus container can scrape it)
- **Configure firewall rules** (UFW/firewalld/iptables) to allow container access
- Add the server to Prometheus targets automatically
- Detect and update binding if Docker network changes

Optional features:

| Variable | Default | Description |
|----------|---------|-------------|
| `WRITE_JSON_FILE` | `off` | Also write logs to JSON file |
| `SMTP_ENABLED` | `false` | Enable email alerting |
| `SMTP_HOST` | - | SMTP server hostname |
| `SMTP_PORT` | `587` | SMTP server port |
| `SMTP_USER` | - | SMTP username |
| `SMTP_PASSWORD` | - | SMTP password |

TLS/mTLS configuration (for encrypted syslog):

| Variable | Default | Description |
|----------|---------|-------------|
| `SYSLOG_TLS_ENABLED` | `false` | Enable TLS on port 6514 |
| `SYSLOG_TLS_HOSTNAME` | TRAEFIK_DOMAIN | Certificate hostname |
| `SYSLOG_TLS_CA_DAYS` | `3650` | CA certificate validity |
| `SYSLOG_TLS_SERVER_DAYS` | `365` | Server cert validity |
| `SYSLOG_TLS_CLIENT_DAYS` | `365` | Client cert validity |
| `SYSLOG_TLS_AUTHMODE` | `anon` | Auth mode (anon/x509/certvalid/x509/name) |
| `SYSLOG_TLS_PERMITTED_PEERS` | `*.hostname` | Allowed client CN patterns (x509/name only) |

### TLS Configuration

#### Syslog TLS (Port 6514)

When TLS is enabled during `init.sh`, the script automatically:
1. Generates a self-signed CA certificate
2. Generates a server certificate signed by the CA
3. Configures rsyslog to accept TLS connections on port 6514

**Authentication Modes:**

| Mode | Description |
|------|-------------|
| `anon` | TLS encryption only, no client certificates required |
| `x509/certvalid` | mTLS - clients must present valid CA-signed certificate |
| `x509/name` | mTLS - clients must have certificate matching PERMITTED_PEERS |

**Generating Client Certificates:**

```bash
# Generate and download client certificate package
rosi-generate-client-cert --download client-hostname

# The package includes: client-key.pem, client-cert.pem, ca.pem
```

#### Traefik TLS (HTTPS)

Traefik automatically obtains Let's Encrypt certificates for HTTPS access.
For local/development without a domain:

1. Comment out the TLS configuration in `docker-compose.yml`
2. Access services directly via their ports

### Log Retention

Loki is configured with:
- 30-day retention period
- Local filesystem storage
- Compaction enabled

Modify `loki-config.yml` to adjust retention settings.

## Troubleshooting

### Logs not appearing in Grafana

1. Check rsyslog is receiving logs:
   ```bash
   docker compose logs rsyslog
   ```

2. Verify rsyslog omhttp is sending to Loki:
   ```bash
   docker compose logs rsyslog | grep -i loki
   ```

3. Check Loki health:
   ```bash
   curl http://localhost:3100/ready
   ```

### Container won't start

```bash
# Check container logs
docker compose logs <service-name>

# Verify disk space
df -h

# Check Docker status
systemctl status docker
```

### Prometheus can't scrape node_exporter (server target down)

The node_exporter on the server must bind to the Docker bridge gateway IP for Prometheus (running inside a container) to reach it.

1. Check current binding:
   ```bash
   grep listen-address /etc/systemd/system/node_exporter.service
   ```

2. Check Docker network gateway:
   ```bash
   rosi-monitor status  # Shows network info and gateway
   # Or manually:
   docker network inspect rosi-collector-net --format '{{range .IPAM.Config}}{{.Gateway}}{{end}}'
   ```

3. Fix binding if mismatched:
   ```bash
   BIND_IP=$(docker network inspect rosi-collector-net --format '{{range .IPAM.Config}}{{.Gateway}}{{end}}')
   sed -i "s/listen-address=[0-9.]*/listen-address=${BIND_IP}/" /etc/systemd/system/node_exporter.service
   systemctl daemon-reload
   systemctl restart node_exporter
   ```

4. Check firewall allows container access:
   ```bash
   # For UFW:
   ufw status | grep 9100
   # Add rule if missing:
   ufw allow from 172.20.0.0/16 to 172.20.0.1 port 9100 proto tcp
   ```

5. Test from Prometheus container:
   ```bash
   docker exec prometheus-central wget -q -O - --timeout=3 http://172.20.0.1:9100/metrics | head -3
   ```

### High memory usage

Loki stores recent logs in memory. Reduce `chunk_idle_period` in `loki-config.yml` for lower memory usage.

## Client Setup

See the `clients/` directory for:

- `rsyslog-forward.conf` - Full rsyslog forwarding config
- `rsyslog-forward-minimal.conf` - Minimal forwarding config
- `install-rsyslog-client.sh` - Automated client setup
- `install-node-exporter.sh` - Prometheus node exporter setup

## Security Considerations

- TLS is enabled by default via Traefik
- Grafana is protected by basic auth
- rsyslog port (10514) should be firewalled to trusted clients
- Prometheus targets file contains client IPs

## Contributing

This is part of the rsyslog project. See the main repository's CONTRIBUTING.md for guidelines.

## License

Apache License 2.0 - See LICENSE file in the rsyslog repository root.
