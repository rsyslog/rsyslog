# AGENTS.md – ROSI Collector Agent Guide

This file provides guidelines for AI assistants working with the ROSI Collector stack (RSyslog Open System for Information).

## Overview

ROSI Collector is a Docker Compose-based centralized log collection stack consisting of:
- **rsyslog** - Log receiver with omhttp output to Loki
- **Grafana Loki** - Log storage and querying
- **Grafana** - Visualization and dashboards
- **Prometheus** - Metrics collection
- **Traefik** - Reverse proxy with automatic TLS

## Directory Structure

```
rosi-collector/
├── docker-compose.yml          # Main stack definition
├── .env.template               # Environment variable template
├── loki-config.yml             # Loki configuration
├── prometheus.yml              # Prometheus base configuration
├── logrotate.conf              # Log rotation for rsyslog
├── README.md                   # User documentation
├── AGENTS.md                   # This file
├── clients/                    # Client setup resources
│   ├── install-rsyslog-client.sh
│   ├── install-node-exporter.sh
│   ├── rsyslog-forward*.conf
│   └── *.md                    # Setup documentation
├── grafana/provisioning/       # Grafana auto-provisioning
│   ├── dashboards/             # JSON dashboard definitions
│   ├── datasources/            # Loki & Prometheus configs
│   └── alerting/               # Alert rule definitions
├── prometheus-targets/         # Dynamic scrape targets
│   └── nodes.yml
├── rsyslog.conf/               # rsyslog configuration files
├── scripts/                    # Management scripts
│   ├── init.sh                 # Environment initialization
│   ├── monitor.sh              # Health monitoring CLI
│   ├── install-server.sh       # Fresh server preparation
│   └── prometheus-target.sh    # Target management CLI
└── traefik/
    └── dynamic.yml.template    # Traefik routing template
```

## Local Testing Setup

### Quick Start (Development/Testing)

```bash
# 1. Navigate to the rosi-collector directory
cd deploy/docker-compose/rosi-collector

# 2. Run init.sh (creates .env and copies files to install directory)
sudo ./scripts/init.sh

# Default install directory: /opt/rosi-collector
# Or specify: sudo INSTALL_DIR=/srv/central ./scripts/init.sh

# 3. Start the stack
cd /opt/rosi-collector  # or your INSTALL_DIR
docker compose up -d

# 4. Check status
docker compose ps
docker compose logs -f
```

### Environment Variables

Required variables (set before running `init.sh` or interactively prompted):
- `TRAEFIK_DOMAIN` - Base domain (e.g., `logs.example.com`)
- `TRAEFIK_EMAIL` - Email for Let's Encrypt certificates

Optional variables:
- `INSTALL_DIR` - Installation directory (default: `/opt/rosi-collector`)
- `GRAFANA_ADMIN_PASSWORD` - Grafana admin password (auto-generated if not set)

### Configuration Persistence

The `init.sh` script stores preferences in:
- User config: `~/.config/rsyslog/rosi-collector.conf`
- System config: `/etc/rsyslog/rosi-collector.conf`

This allows subsequent runs to remember the install directory.

## Testing Changes

### After Modifying Configuration Files

```bash
# Copy updated files to install directory
cp -r /path/to/rosi-collector/* /opt/rosi-collector/

# Restart affected service(s)
cd /opt/rosi-collector
docker compose restart grafana      # For dashboard/datasource changes
docker compose restart loki         # For loki-config.yml changes
docker compose restart prometheus   # For prometheus.yml changes
docker compose restart rsyslog      # For rsyslog.conf/ changes

# Or restart entire stack
docker compose down && docker compose up -d
```

### After Modifying docker-compose.yml

```bash
cd /opt/rosi-collector
docker compose up -d  # Recreates changed containers
```

### Testing Grafana Dashboards

1. Edit JSON files in `grafana/provisioning/dashboards/`
2. Copy to install directory
3. Restart Grafana: `docker compose restart grafana`
4. Access Grafana at `https://grafana.TRAEFIK_DOMAIN/` or `http://localhost:3000`

### Testing Alert Rules

1. Edit `grafana/provisioning/alerting/default.yml`
2. Copy and restart Grafana
3. Check Grafana → Alerting → Alert rules

## Common Operations

### View Container Logs

```bash
docker compose logs -f              # All containers
docker compose logs -f grafana      # Specific container
docker compose logs --tail=50 loki  # Last 50 lines
```

### Check Service Health

```bash
# Use the monitor script (if installed)
rosi-monitor                        # Interactive menu
rosi-monitor --check               # Quick health check

# Or manually
docker compose ps
curl -s http://localhost:3100/ready  # Loki health
curl -s http://localhost:3000/api/health  # Grafana health
```

### Manage Prometheus Targets

```bash
# Use the helper script
prometheus-target list
prometheus-target add 10.0.0.5:9100 host=webserver role=web
prometheus-target remove 10.0.0.5:9100
```

### Reset Grafana (Fresh Database)

If Grafana has database issues (e.g., alert rule conflicts after version change):

```bash
docker compose stop grafana
docker volume rm $(docker volume ls -q | grep '_grafana-data$')
docker compose start grafana
```

## Important Files for Agents

### docker-compose.yml
- Main stack definition
- Image versions should be pinned (not `latest` for production)
- Environment variables from `.env` file

### loki-config.yml
- `retention_period`: Log retention (default: 720h = 30 days)
- `max_query_*`: Query limits for performance
- Must match documentation claims

### grafana/provisioning/dashboards/*.json
- Dashboard UID in `"uid"` field at end of file
- Used for home dashboard configuration
- Provisioned on Grafana startup

### scripts/init.sh
- Creates `.env` from template
- Copies files to `INSTALL_DIR`
- Sets up systemd service
- Installs CLI tools (rosi-monitor, prometheus-target)

## Troubleshooting

### Container Won't Start

```bash
# Check logs
docker compose logs <service-name>

# Common issues:
# - Port already in use: Check `netstat -tlnp | grep <port>`
# - Volume permissions: Check ownership matches container user
# - Config syntax: Validate YAML files
```

### Grafana Alert Provisioning Fails

Error: `UNIQUE constraint failed: alert_rule.guid`

Solution: Clear alert tables or reset Grafana database (see Reset Grafana above)

### Prometheus Permission Denied

Error: `open /prometheus/queries.active: permission denied`

Solution: Fix volume ownership:
```bash
docker volume inspect <prometheus-volume> | grep Mountpoint
sudo chown -R 65534:65534 /path/to/volume/_data
```

### Loki Not Receiving Logs

1. Check rsyslog container logs: `docker compose logs rsyslog`
2. Verify Loki is healthy: `curl http://localhost:3100/ready`
3. Check rsyslog config in `rsyslog.conf/30-send-loki-http.conf`

## Code Style Guidelines

### Shell Scripts
- Use `#!/usr/bin/env bash`
- Enable strict mode: `set -euo pipefail`
- Use functions for modularity
- Include help/usage information
- Handle errors gracefully with meaningful messages

### YAML Configuration
- Use 2-space indentation
- Include comments explaining non-obvious settings
- Keep consistent with upstream documentation

### Docker Compose
- Pin image versions for production stability
- Use named volumes for persistent data
- Include health checks where applicable
- Document environment variables in `.env.template`

## Related Documentation

- Main README: `README.md`
- Client setup: `clients/README.md`
- RSyslog client: `clients/RSYSLOG-SETUP.md`
- Node Exporter: `clients/NODE-EXPORTER-SETUP.md`
- RST docs: `doc/source/deployments/rosi_collector/`

## Security Considerations

1. **No hardcoded credentials** - Use environment variables and `.env` file
2. **Pin Docker image versions** - Avoid `latest` tag in production
3. **TLS certificates** - Traefik handles Let's Encrypt automatically
4. **Firewall rules** - Restrict access to management ports
5. **No insecure downloads** - Don't use `--insecure` or `--no-check-certificate`
6. **Non-root containers** - Services should run as non-root where possible
