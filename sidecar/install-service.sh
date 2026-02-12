#!/usr/bin/env bash
## install-service.sh: Install rsyslog exporter as a systemd service.
##
## Usage:
##   sudo ./install-service.sh [--prefix /opt/rsyslog-exporter] \

##     [--user rsyslog] [--group rsyslog] \
##     [--listen-addr 127.0.0.1] [--listen-port 9898] \
##     [--select-listen-addr] \
##     [--overwrite] \
##     [--impstats-mode udp] [--impstats-format json] \
##     [--impstats-udp-addr 127.0.0.1] [--impstats-udp-port 19090]
##
## This script:
##   - installs the exporter into a fixed prefix
##   - installs Python deps into a venv under the prefix
##   - installs a systemd unit file and enables the service

set -euo pipefail

PREFIX="/opt/rsyslog-exporter"
RUN_USER="rsyslog"
RUN_GROUP="rsyslog"
LISTEN_ADDR="127.0.0.1"
LISTEN_PORT="9898"
LISTEN_ADDR_SET="false"
IMPSTATS_MODE="udp"
IMPSTATS_FORMAT="json"
IMPSTATS_UDP_ADDR="127.0.0.1"
IMPSTATS_UDP_PORT="19090"
IMPSTATS_UDP_ADDR_SET="false"
IMPSTATS_UDP_PORT_SET="false"
SELECT_LISTEN_ADDR="false"
OVERWRITE="false"
RSYSLOG_CONF_SOURCE=""
RSYSLOG_CONF_TARGET="/etc/rsyslog.d/10-impstats.conf"

prompt_yes_no() {
  local prompt="$1"
  local reply

  read -rp "$prompt" reply
  case "${reply:-}" in
    [Yy]|[Yy][Ee][Ss]) return 0 ;;
    *) return 1 ;;
  esac
}

list_ipv4_addrs() {
  local addrs=()

  if command -v ip >/dev/null 2>&1; then
    while IFS= read -r line; do
      addrs+=("${line%%/*}")
    done < <(ip -4 -o addr show | awk '{print $4}')
  elif command -v ifconfig >/dev/null 2>&1; then
    while IFS= read -r line; do
      addrs+=("${line}")
    done < <(ifconfig 2>/dev/null | awk '/inet / && !/inet6/ {print $2}' | sed 's/addr://')
  fi

  # Always include loopback and all-interfaces
  addrs+=("127.0.0.1" "0.0.0.0")

  # Deduplicate
  printf "%s\n" "${addrs[@]}" | awk '!seen[$0]++'
}

select_listen_addr() {
  if [ ! -t 0 ]; then
    echo "Error: --select-listen-addr requires an interactive terminal" >&2
    exit 1
  fi

  mapfile -t choices < <(list_ipv4_addrs)
  if [ ${#choices[@]} -eq 0 ]; then
    echo "Error: No IPv4 addresses found" >&2
    exit 1
  fi

  echo "Available listen addresses:"
  for i in "${!choices[@]}"; do
    printf "  [%d] %s\n" "$((i+1))" "${choices[$i]}"
  done
  echo ""
  read -rp "Select listen address [1-${#choices[@]}]: " selection
  if ! [[ "$selection" =~ ^[0-9]+$ ]] || [ "$selection" -lt 1 ] || [ "$selection" -gt ${#choices[@]} ]; then
    echo "Error: Invalid selection" >&2
    exit 1
  fi

  LISTEN_ADDR="${choices[$((selection-1))]}"
  LISTEN_ADDR_SET="true"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix) PREFIX="$2"; shift 2 ;;
    --user) RUN_USER="$2"; shift 2 ;;
    --group) RUN_GROUP="$2"; shift 2 ;;
    --listen-addr) LISTEN_ADDR="$2"; LISTEN_ADDR_SET="true"; shift 2 ;;
    --listen-port) LISTEN_PORT="$2"; shift 2 ;;
    --select-listen-addr) SELECT_LISTEN_ADDR="true"; shift 1 ;;
    --overwrite) OVERWRITE="true"; shift 1 ;;
    --impstats-mode) IMPSTATS_MODE="$2"; shift 2 ;;
    --impstats-format) IMPSTATS_FORMAT="$2"; shift 2 ;;
    --impstats-udp-addr) IMPSTATS_UDP_ADDR="$2"; IMPSTATS_UDP_ADDR_SET="true"; shift 2 ;;
    --impstats-udp-port) IMPSTATS_UDP_PORT="$2"; IMPSTATS_UDP_PORT_SET="true"; shift 2 ;;
    *) echo "Unknown argument: $1"; exit 2 ;;
  esac
done

if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root." >&2
  exit 1
fi

if [ "$OVERWRITE" = "true" ]; then
  safe_prefix="${PREFIX%/}"
  if [ -z "$safe_prefix" ]; then
    safe_prefix="/"
  fi
  case "$safe_prefix" in
    /|/bin|/sbin|/usr|/etc|/var|/lib|/lib64|/boot|/dev|/proc|/sys|/run|/root|/home)
      echo "Refusing to overwrite unsafe prefix: $PREFIX" >&2
      exit 1
      ;;
  esac
fi

if [ "$SELECT_LISTEN_ADDR" = "true" ]; then
  select_listen_addr
fi

if [ "$IMPSTATS_UDP_ADDR_SET" = "false" ] && [ "$LISTEN_ADDR_SET" = "true" ] && [ "$LISTEN_ADDR" != "0.0.0.0" ]; then
  IMPSTATS_UDP_ADDR="$LISTEN_ADDR"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RSYSLOG_CONF_SOURCE="$SCRIPT_DIR/examples/rsyslog-impstats-udp.conf"

install -d -m 0755 "$PREFIX"
install -d -m 0755 "$PREFIX/bin"
install -d -m 0755 "$PREFIX/venv"
install -d -m 0755 /var/log/rsyslog-exporter
install -d -m 0755 /run/rsyslog-exporter

if [ -t 0 ] && [ -f "$RSYSLOG_CONF_SOURCE" ]; then
  echo "Optional rsyslog impstats config deployment:"
  echo "  Source: $RSYSLOG_CONF_SOURCE"
  echo "  Target: $RSYSLOG_CONF_TARGET"
  if [ "$IMPSTATS_MODE" != "udp" ]; then
    echo "  Note: The bundled config is for UDP mode."
  fi
  rsyslog_target_addr="$IMPSTATS_UDP_ADDR"
  if [ "$IMPSTATS_UDP_ADDR_SET" = "false" ] && [ "$LISTEN_ADDR_SET" = "true" ] && [ "$LISTEN_ADDR" != "0.0.0.0" ]; then
    rsyslog_target_addr="$LISTEN_ADDR"
  fi
  rsyslog_target_port="$IMPSTATS_UDP_PORT"
  echo "  Using target ${rsyslog_target_addr}:${rsyslog_target_port}"
  if prompt_yes_no "Deploy this config to $RSYSLOG_CONF_TARGET? [y/N]: "; then
    rsyslog_conf_tmp="$(mktemp)"
    sed -e "s/target=\"[^\"]*\"/target=\"${rsyslog_target_addr}\"/" \
        -e "s/port=\"[^\"]*\"/port=\"${rsyslog_target_port}\"/" \
        "$RSYSLOG_CONF_SOURCE" > "$rsyslog_conf_tmp"
    if [ -e "$RSYSLOG_CONF_TARGET" ]; then
      if prompt_yes_no "File exists. Overwrite $RSYSLOG_CONF_TARGET? [y/N]: "; then
        install -m 0644 "$rsyslog_conf_tmp" "$RSYSLOG_CONF_TARGET"
      else
        echo "Skipping rsyslog config deployment."
      fi
    else
      install -m 0644 "$rsyslog_conf_tmp" "$RSYSLOG_CONF_TARGET"
    fi
    rm -f "$rsyslog_conf_tmp"
  else
    echo "Skipping rsyslog config deployment."
  fi
else
  echo "Skipping rsyslog config deployment (non-interactive or missing template)."
fi

if ! id -u "$RUN_USER" >/dev/null 2>&1; then
  useradd --system --no-create-home --shell /usr/sbin/nologin "$RUN_USER"
fi
if ! getent group "$RUN_GROUP" >/dev/null 2>&1; then
  groupadd --system "$RUN_GROUP"
fi

chown -R "$RUN_USER:$RUN_GROUP" /var/log/rsyslog-exporter /run/rsyslog-exporter

if [ "$OVERWRITE" = "true" ]; then
  if systemctl is-active --quiet rsyslog-exporter; then
    systemctl stop rsyslog-exporter
  fi
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete --exclude 'venv' --exclude '.git' "$SCRIPT_DIR/" "$PREFIX/"
  else
    find "${PREFIX:?}" -mindepth 1 -maxdepth 1 -exec rm -rf -- {} +
    cp -a "$SCRIPT_DIR"/* "$PREFIX/"
  fi
else
  cp -a "$SCRIPT_DIR"/* "$PREFIX/"
fi

if [[ ! -x "$PREFIX/venv/bin/python" ]]; then
  python3 -m venv "$PREFIX/venv"
fi
if [[ ! -x "$PREFIX/venv/bin/python" ]]; then
  echo "Failed to create venv at $PREFIX/venv" >&2
  exit 1
fi
"$PREFIX/venv/bin/python" -m ensurepip --upgrade
"$PREFIX/venv/bin/python" -m pip install --upgrade pip
"$PREFIX/venv/bin/python" -m pip install -r "$PREFIX/requirements.txt"

# Service runs as RUN_USER; venv must be readable/executable by that user
chown -R "$RUN_USER:$RUN_GROUP" "$PREFIX"

cat > /etc/systemd/system/rsyslog-exporter.service <<EOF
[Unit]
Description=rsyslog Prometheus Exporter
Documentation=https://github.com/rsyslog/rsyslog
After=network.target rsyslog.service
Wants=rsyslog.service

[Service]
Type=simple
User=${RUN_USER}
Group=${RUN_GROUP}
WorkingDirectory=${PREFIX}

ExecStart=/bin/sh -c "\\
  ${PREFIX}/venv/bin/python -m gunicorn \\
    --bind \${LISTEN_ADDR}:\${LISTEN_PORT} \\
    --workers 1 \\
    --threads 2 \\
    --timeout 30 \\
    --access-logfile /var/log/rsyslog-exporter/access.log \\
    --error-logfile /var/log/rsyslog-exporter/error.log \\
    --log-level info \\
    --pid /run/rsyslog-exporter/rsyslog-exporter.pid \\
    rsyslog_exporter:application"

ExecReload=/bin/kill -s HUP \$MAINPID

Environment="IMPSTATS_MODE=${IMPSTATS_MODE}"
Environment="IMPSTATS_FORMAT=${IMPSTATS_FORMAT}"
Environment="IMPSTATS_UDP_ADDR=${IMPSTATS_UDP_ADDR}"
Environment="IMPSTATS_UDP_PORT=${IMPSTATS_UDP_PORT}"
Environment="LISTEN_ADDR=${LISTEN_ADDR}"
Environment="LISTEN_PORT=${LISTEN_PORT}"
Environment="LOG_LEVEL=INFO"
Environment="MAX_BURST_BUFFER_LINES=10000"
Environment="ALLOWED_UDP_SOURCES=${ALLOWED_UDP_SOURCES:-}"

Restart=on-failure
RestartSec=5s
KillMode=mixed
KillSignal=SIGTERM

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/rsyslog-exporter
RuntimeDirectory=rsyslog-exporter

MemoryMax=256M
TasksMax=50

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable rsyslog-exporter
systemctl restart rsyslog-exporter
systemctl status rsyslog-exporter --no-pager

echo "Installed and started rsyslog-exporter.service"
