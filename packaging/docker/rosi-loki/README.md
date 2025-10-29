# ROSI Loki PoC Stack

This proof of concept wiring makes it easy to visualize rsyslog/ROSI logs in Grafana via Promtail's syslog receiver.

## Prerequisites

- Docker
- Docker Compose v2

## Start the stack

Set a Grafana admin password via environment variable (either export it or
place it in a `.env` file) before starting the stack:

```bash
export GRAFANA_ADMIN_PASSWORD='choose-a-strong-password'
```

Alternatively create a `.env` file in this directory with the same variable so
Docker Compose picks it up automatically.

```bash
cd packaging/docker/rosi-loki
docker compose up -d
```

## Send a test message from the host

```bash
logger -n 127.0.0.1 -P 1514 -T "hello loki"
```

## Access Grafana

- URL: http://localhost:3000
- Credentials: `admin` / password from `GRAFANA_ADMIN_PASSWORD` (defaults to
  `admin`; Grafana forces a password change on first login)
- Explore query: `{job="syslog"}`

## Basic troubleshooting

- Inspect container logs with `docker logs <name>`
- Verify ports 3000 (Grafana), 3100 (Loki), and 1514/tcp (Promtail syslog) are reachable
- Check local firewall or SELinux rules if the syslog message does not arrive
- Promtail stores its positions file under `/var/lib/promtail`, persisted via a
  named Docker volume

## Next steps

- Configure rsyslog omhttp dual-write into Loki in parallel with ROSI
- Add TLS and authentication before exposing the stack beyond local PoC use
