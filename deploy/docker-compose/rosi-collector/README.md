# ROSI Collector

**RSyslog Open System for Information** - A production-ready centralized log collection and monitoring stack.

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
│  │             (log receiver, TCP 10514)                     │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
         ▲                                        ▲
         │ logs (TCP 10514)                       │ metrics (9100)
         │                                        │
┌────────┴────────┐                      ┌────────┴────────┐
│   Client Host   │                      │   Client Host   │
│  (rsyslog +     │  ...                 │  (rsyslog +     │
│  node-exporter) │                      │  node-exporter) │
└─────────────────┘                      └─────────────────┘
```

## Quick Start

### Prerequisites

- Docker Engine 20.10+
- Docker Compose v2
- Linux host with 2GB+ RAM
- Domain name (for TLS) or local deployment

### 1. Clone Repository

```bash
git clone https://github.com/rsyslog/rsyslog.git
cd rsyslog/deploy/docker-compose/rosi-collector
```

### 2. Initialize Environment

Run the initialization script (as root):

```bash
sudo TRAEFIK_DOMAIN=logs.example.com \
     TRAEFIK_EMAIL=admin@example.com \
     ./scripts/init.sh
```

This script will:
- Prompt for installation directory (default: `/opt/rosi-collector`)
- Copy all configuration files
- Generate `.env` with secure passwords
- Create Docker network
- Set up systemd service

**Tip**: For custom Grafana password, add `GRAFANA_ADMIN_PASSWORD=your-password` to the command.

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

- Open `https://grafana.logs.example.com` (your configured domain)
- Login with `admin` / password from `.env` file:
  ```bash
  grep GRAFANA_ADMIN_PASSWORD /opt/rosi-collector/.env
  ```
- Pre-configured dashboards are available under "Dashboards"

### 5. Configure Clients

On each client host, forward logs to your ROSI Collector:

```bash
# Download and run client setup script
wget https://your-domain.com/downloads/install-rsyslog-client.sh
sudo bash install-rsyslog-client.sh
```

See `clients/README.md` for detailed client setup instructions.

## Services

| Service | Port | Description |
|---------|------|-------------|
| Traefik | 80, 443 | Reverse proxy with automatic TLS |
| Grafana | 3000 | Log visualization and dashboards |
| Loki | 3100 | Log storage and querying |
| Prometheus | 9090 | Metrics collection |
| rsyslog | 10514 | Log receiver (TCP) with omhttp output |

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
├── grafana/                    # Grafana provisioning
│   └── provisioning/
│       ├── dashboards/         # Pre-built dashboards
│       ├── datasources/        # Data source configs
│       └── alerting/           # Alert rules
├── scripts/                    # Management scripts
│   ├── init.sh                 # Initialize environment
│   ├── monitor.sh              # Health monitoring
│   └── prometheus-target.sh    # Manage scrape targets
└── clients/                    # Client setup resources
```

## Pre-built Dashboards

ROSI Collector includes several pre-configured Grafana dashboards:

| Dashboard | Description |
|-----------|-------------|
| Syslog Explorer | Search and browse logs |
| Syslog Deep Dive | Detailed log analysis |
| Node Overview | System metrics overview |
| Client Health | rsyslog client status |
| Alerting Overview | Active alerts status |

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

### Prepare Fresh Server (Optional)

⚠️ **WARNING**: Only run this on fresh/new systems. Do NOT run on servers you maintain yourself - it modifies system configuration, firewall rules, and installs packages.

For fresh Ubuntu 24.04 systems, this script installs Docker, configures firewall, and prepares the server:

```bash
# Basic installation
sudo ./scripts/install-server.sh

# With custom settings
sudo SSH_PORT=22 \
     OPEN_TCP_PORTS="80 443 10514" \
     HOSTNAME_FQDN="rosi.example.com" \
     ./scripts/install-server.sh
```

The script:
- Installs Docker CE and dependencies
- Configures UFW firewall
- Sets Docker logging to syslog
- Configures hostname (optional)

### Monitor Stack Health

After running `init.sh`, the `rosi-monitor` command is available system-wide:

```bash
# Show container status
rosi-monitor status

# Show recent logs
rosi-monitor logs

# Health check
rosi-monitor health

# Interactive debug menu
rosi-monitor debug
```

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

# Add with multiple labels
prometheus-target add 10.0.0.5:9100 host=webserver role=web env=production

# List all configured targets
prometheus-target list

# Remove a target
prometheus-target remove 10.0.0.5:9100
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

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TRAEFIK_DOMAIN` | - | Main domain for the stack |
| `TRAEFIK_EMAIL` | - | Let's Encrypt email |
| `GRAFANA_ADMIN_PASSWORD` | (generated) | Grafana admin password |
| `GRAFANA_DOMAIN` | `grafana.${TRAEFIK_DOMAIN}` | Grafana subdomain |
| `WRITE_JSON_FILE` | `off` | Also write logs to JSON |
| `SMTP_ENABLED` | `false` | Enable email alerting |
| `INSTALL_DIR` | `/opt/rosi-collector` | Installation directory |

### TLS Configuration

Traefik automatically obtains Let's Encrypt certificates. For local/development:

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

### High memory usage

Loki stores recent logs in memory. Reduce `chunk_idle_period` in `loki-config.yml` for lower memory usage.

## Client Setup

See the `clients/` directory for:

- `rsyslog-forward.conf` - Full rsyslog forwarding config
- `rsyslog-forward-minimal.conf` - Minimal forwarding config
- `rsyslog-impstats.conf` - rsyslog statistics for monitoring
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
