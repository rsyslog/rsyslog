#!/usr/bin/env bash
# init.sh
# Creates and updates Docker Compose environment for ROSI Collector (Loki/Grafana/Prometheus) stack
# Part of RSyslog Open System for Information (ROSI)

set -euo pipefail

# Configuration file locations (XDG Base Directory Specification)
# System-wide config: /etc/rsyslog/rosi-collector.conf
# User config: ~/.config/rsyslog/rosi-collector.conf
SYSTEM_CONFIG="/etc/rsyslog/rosi-collector.conf"
USER_CONFIG="${HOME}/.config/rsyslog/rosi-collector.conf"

# Load configuration from file if exists
load_config() {
    local config_file="$1"
    if [ -f "$config_file" ]; then
        # Source config file (only export known variables for safety)
        while IFS='=' read -r key value; do
            # Skip comments and empty lines
            [[ "$key" =~ ^#.*$ ]] && continue
            [[ -z "$key" ]] && continue
            # Remove quotes from value
            value="${value%\"}"
            value="${value#\"}"
            value="${value%\'}"
            value="${value#\'}"
            # Only allow specific variables
            case "$key" in
                INSTALL_DIR|TRAEFIK_DOMAIN|TRAEFIK_EMAIL|GRAFANA_DOMAIN)
                    export "$key=$value"
                    ;;
            esac
        done < "$config_file"
        return 0
    fi
    return 1
}

# Save configuration to file
save_config() {
    local config_file="$1"
    local install_dir="$2"
    
    mkdir -p "$(dirname "$config_file")"
    cat > "$config_file" << EOF
# ROSI Collector local configuration
# This file stores your local installation preferences
# Generated on $(date -Iseconds)

INSTALL_DIR="${install_dir}"
EOF
    chmod 600 "$config_file"
    echo "Saved configuration to: $config_file"
}

# Prompt for install directory if not set
prompt_install_dir() {
    local default_dir="/opt/rosi-collector"
    local current_dir="${INSTALL_DIR:-}"
    
    # If INSTALL_DIR is already set via environment, use it
    if [ -n "$current_dir" ]; then
        echo "Using INSTALL_DIR from environment: $current_dir"
        return 0
    fi
    
    # Try to load from config files (user config takes precedence)
    if load_config "$USER_CONFIG"; then
        echo "Loaded configuration from: $USER_CONFIG"
        return 0
    fi
    
    if load_config "$SYSTEM_CONFIG"; then
        echo "Loaded configuration from: $SYSTEM_CONFIG"
        return 0
    fi
    
    # Interactive prompt if running in a terminal
    if [ -t 0 ]; then
        echo ""
        echo "ROSI Collector Installation Directory"
        echo "======================================"
        echo "Default: $default_dir"
        echo ""
        read -rp "Enter installation directory [$default_dir]: " user_input
        
        if [ -n "$user_input" ]; then
            INSTALL_DIR="$user_input"
        else
            INSTALL_DIR="$default_dir"
        fi
        
        # Ask if user wants to save this preference
        read -rp "Save this preference for future runs? [Y/n]: " save_pref
        case "$save_pref" in
            [Nn]*)
                echo "Configuration not saved (will ask again next time)"
                ;;
            *)
                save_config "$USER_CONFIG" "$INSTALL_DIR"
                ;;
        esac
    else
        # Non-interactive: use default
        INSTALL_DIR="$default_dir"
        echo "Using default installation directory: $INSTALL_DIR"
    fi
    
    export INSTALL_DIR
}

# Run directory prompt/config loading
prompt_install_dir

STACK="rosi-collector"
BASE="${INSTALL_DIR}"
NET="rosi-collector-net"
COMPOSE="${BASE}/docker-compose.yml"
ENVFILE="${BASE}/.env"
TZVAL="$(cat /etc/timezone 2>/dev/null || echo UTC)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIGS_DIR="${SCRIPT_DIR}/.."  # Config files are in parent directory

need_root() { [ "${EUID:-$(id -u)}" -eq 0 ] || { echo "Run as root."; exit 1; }; }
mkuser() {
  if ! id -u "${STACK}" >/dev/null 2>&1; then
    adduser --system --group --home "${BASE}" --shell /usr/sbin/nologin "${STACK}"
  fi
}
genpw() {
  openssl rand -base64 48 | tr -d '\n' | head -c 32
}
gen_htpasswd() {
  local user="$1"
  local pass="$2"
  # Generate htpasswd with bcrypt (openssl passwd -apr1 for Apache MD5, but we use Python for bcrypt)
  # Fallback to openssl if Python not available
  if command -v python3 >/dev/null 2>&1; then
    python3 -c "import bcrypt; print('${user}:' + bcrypt.hashpw('${pass}'.encode(), bcrypt.gensalt()).decode())" 2>/dev/null && return
  fi
  # Fallback to htpasswd if available
  if command -v htpasswd >/dev/null 2>&1; then
    echo "${pass}" | htpasswd -niB "${user}" 2>/dev/null && return
  fi
  # Fallback to openssl (less secure but widely available)
  echo "${user}:$(openssl passwd -apr1 "${pass}")"
}

need_root
umask 027
mkuser

if [ ! -f "${CONFIGS_DIR}/docker-compose.yml" ]; then
  echo "Error: docker-compose.yml not found in: ${CONFIGS_DIR}" >&2
  echo "Run this script from the rosi-collector directory" >&2
  exit 1
fi

install -d -m 0750 "${BASE}"
chown ${STACK}:${STACK} "${BASE}"

echo "Copying configuration files to ${BASE}..."

rsync -a --exclude='*.template' --exclude='prometheus-targets/' "${CONFIGS_DIR}/" "${BASE}/" || {
  echo "Error: Failed to copy config files" >&2
  exit 1
}

# Create prometheus-targets directory if it doesn't exist (first run)
if [ ! -d "${BASE}/prometheus-targets" ]; then
  install -d -m 0755 "${BASE}/prometheus-targets"
  cp "${CONFIGS_DIR}/prometheus-targets/nodes.yml" "${BASE}/prometheus-targets/"
  echo "Created prometheus-targets directory with template"
fi

download_grafana_dashboard() {
  local dashboard_id="$1"
  local output_file="${DASHBOARDS_DST}/grafana-${dashboard_id}.json"
  local url="https://grafana.com/api/dashboards/${dashboard_id}/revisions/latest/download"
  
  if [ -f "${output_file}" ]; then
    echo "Dashboard ${dashboard_id} already exists, skipping download"
    return 0
  fi
  
  echo "Downloading Grafana dashboard ${dashboard_id}..."
  if command -v curl >/dev/null 2>&1; then
    if curl -sSLf -o "${output_file}" "${url}"; then
      chmod 644 "${output_file}"
      echo "Successfully downloaded dashboard ${dashboard_id}"
      return 0
    else
      echo "Warning: Failed to download dashboard ${dashboard_id}" >&2
      rm -f "${output_file}"
      return 1
    fi
  elif command -v wget >/dev/null 2>&1; then
    if wget -q --spider "${url}" 2>/dev/null && wget -q -O "${output_file}" "${url}"; then
      chmod 644 "${output_file}"
      echo "Successfully downloaded dashboard ${dashboard_id}"
      return 0
    else
      echo "Warning: Failed to download dashboard ${dashboard_id}" >&2
      rm -f "${output_file}"
      return 1
    fi
  else
    echo "Warning: Neither curl nor wget found, cannot download dashboard ${dashboard_id}" >&2
    return 1
  fi
}

DASHBOARDS_SRC="${CONFIGS_DIR}/grafana/provisioning/dashboards"
DASHBOARDS_DST="${BASE}/grafana/provisioning/dashboards"
install -d -m 0755 "${DASHBOARDS_DST}"

if [ -d "${DASHBOARDS_SRC}" ]; then
  echo "Installing local Grafana dashboards..."
  rsync -a --delete "${DASHBOARDS_SRC}/" "${DASHBOARDS_DST}/" || {
    echo "Warning: Failed to copy dashboards" >&2
  }
fi

echo "Downloading Grafana dashboards from grafana.com..."
# Node Exporter Full
download_grafana_dashboard 1860
# Loki stack monitoring
download_grafana_dashboard 14055

find "${DASHBOARDS_DST}" -name "*.json" -type f -exec chmod 644 {} \;
DASHBOARD_COUNT=$(find "${DASHBOARDS_DST}" -name "*.json" -type f | wc -l)
echo "Total dashboards installed: ${DASHBOARD_COUNT}"

if [ -d "${CONFIGS_DIR}/downloads" ]; then
  echo "Installing download files..."
  install -d -m 0755 "${BASE}/downloads"
  rsync -a "${CONFIGS_DIR}/downloads/" "${BASE}/downloads/" || {
    echo "Warning: Failed to copy download files" >&2
  }
  
  CLIENTS_DIR="${CONFIGS_DIR}/clients"
  if [ -f "${CLIENTS_DIR}/rsyslog-forward-minimal.conf" ]; then
    echo "Copying rsyslog client config to downloads..."
    cp "${CLIENTS_DIR}/rsyslog-forward-minimal.conf" "${BASE}/downloads/" || {
      echo "Warning: Failed to copy rsyslog-forward-minimal.conf" >&2
    }
  fi
  
  if [ -f "${ENVFILE}" ]; then
    TRAEFIK_DOMAIN=$(grep "^TRAEFIK_DOMAIN=" "${ENVFILE}" | cut -d'=' -f2- | tr -d '"' || echo "")
    GRAFANA_DOMAIN=$(grep "^GRAFANA_DOMAIN=" "${ENVFILE}" | cut -d'=' -f2- | tr -d '"' || echo "")
  fi
  
  TRAEFIK_DOMAIN="${TRAEFIK_DOMAIN:-example.com}"
  GRAFANA_DOMAIN="${GRAFANA_DOMAIN:-grafana.${TRAEFIK_DOMAIN}}"
  
  echo "Substituting domain placeholders in download scripts..."
  find "${BASE}/downloads" -type f -name "*.sh" -exec sed -i \
    -e "s|__GRAFANA_DOMAIN__|${GRAFANA_DOMAIN}|g" \
    -e "s|__TRAEFIK_DOMAIN__|${TRAEFIK_DOMAIN}|g" \
    {} \;
  
  find "${BASE}/downloads" -type f -exec chmod 644 {} \;
  find "${BASE}/downloads" -type f -name "*.sh" -exec chmod +x {} \;
  echo "Download files installed"
fi

install -d -m 0750 "${BASE}/traefik/letsencrypt"
chown -R ${STACK}:${STACK} "${BASE}"

find "${BASE}" -type f -name "*.sh" -exec chmod +x {} \;


if [ ! -f "${ENVFILE}" ]; then
  ENV_TEMPLATE="${CONFIGS_DIR}/.env.template"
  if [ ! -f "${ENV_TEMPLATE}" ]; then
    echo "Error: Template file not found: ${ENV_TEMPLATE}" >&2
    exit 1
  fi
  
  TRAEFIK_DOMAIN="${TRAEFIK_DOMAIN:-example.com}"
  TRAEFIK_EMAIL="${TRAEFIK_EMAIL:-admin@${TRAEFIK_DOMAIN}}"
  GRAFANA_ADMIN_PASSWORD="${GRAFANA_ADMIN_PASSWORD:-$(genpw)}"
  GRAFANA_DOMAIN="${GRAFANA_DOMAIN:-grafana.${TRAEFIK_DOMAIN}}"
  GRAFANA_ROOT_URL="${GRAFANA_ROOT_URL:-https://${GRAFANA_DOMAIN}/}"
  WRITE_JSON_FILE="${WRITE_JSON_FILE:-off}"
  SMTP_ENABLED="${SMTP_ENABLED:-false}"
  SMTP_HOST="${SMTP_HOST:-}"
  SMTP_PORT="${SMTP_PORT:-587}"
  SMTP_USER="${SMTP_USER:-}"
  SMTP_PASSWORD="${SMTP_PASSWORD:-}"
  ALERT_EMAIL_FROM="${ALERT_EMAIL_FROM:-alerts@${TRAEFIK_DOMAIN}}"
  ALERT_EMAIL_TO="${ALERT_EMAIL_TO:-${TRAEFIK_EMAIL}}"
  SMTP_SKIP_VERIFY="${SMTP_SKIP_VERIFY:-false}"
  
  sed -e "s|__TRAEFIK_DOMAIN__|${TRAEFIK_DOMAIN}|g" \
      -e "s|__TRAEFIK_EMAIL__|${TRAEFIK_EMAIL}|g" \
      -e "s|__GRAFANA_ADMIN_PASSWORD__|${GRAFANA_ADMIN_PASSWORD}|g" \
      -e "s|__GRAFANA_DOMAIN__|${GRAFANA_DOMAIN}|g" \
      -e "s|__GRAFANA_ROOT_URL__|${GRAFANA_ROOT_URL}|g" \
      -e "s|__WRITE_JSON_FILE__|${WRITE_JSON_FILE}|g" \
      -e "s|__SMTP_ENABLED__|${SMTP_ENABLED}|g" \
      -e "s|__SMTP_HOST__|${SMTP_HOST}|g" \
      -e "s|__SMTP_PORT__|${SMTP_PORT}|g" \
      -e "s|__SMTP_USER__|${SMTP_USER}|g" \
      -e "s|__SMTP_PASSWORD__|${SMTP_PASSWORD}|g" \
      -e "s|__ALERT_EMAIL_FROM__|${ALERT_EMAIL_FROM}|g" \
      -e "s|__ALERT_EMAIL_TO__|${ALERT_EMAIL_TO}|g" \
      -e "s|__SMTP_SKIP_VERIFY__|${SMTP_SKIP_VERIFY}|g" \
      "${ENV_TEMPLATE}" > "${ENVFILE}"
  chmod 600 "${ENVFILE}"
  chown ${STACK}:${STACK} "${ENVFILE}"
  echo "Created .env file with generated passwords"
else
  echo ".env file already exists, skipping generation"
fi

# Generate Traefik dynamic config with basic auth credentials
TRAEFIK_DYNAMIC="${BASE}/traefik/dynamic.yml"
TRAEFIK_DYNAMIC_TEMPLATE="${CONFIGS_DIR}/traefik/dynamic.yml.template"
if [ -f "${TRAEFIK_DYNAMIC_TEMPLATE}" ]; then
  # Read GRAFANA_ADMIN_PASSWORD from .env file
  ADMIN_PASS=$(grep "^GRAFANA_ADMIN_PASSWORD=" "${ENVFILE}" | cut -d'=' -f2- | tr -d '"')
  if [ -n "${ADMIN_PASS}" ]; then
    HTPASSWD_CREDS=$(gen_htpasswd "admin" "${ADMIN_PASS}")
    # Escape special characters for sed ($ needs escaping in htpasswd output)
    HTPASSWD_ESCAPED=$(echo "${HTPASSWD_CREDS}" | sed 's/\$/\\$/g')
    sed -e "s|__BASIC_AUTH_CREDENTIALS__|${HTPASSWD_ESCAPED}|g" \
        "${TRAEFIK_DYNAMIC_TEMPLATE}" > "${TRAEFIK_DYNAMIC}"
    chmod 640 "${TRAEFIK_DYNAMIC}"
    chown ${STACK}:${STACK} "${TRAEFIK_DYNAMIC}"
    echo "Generated Traefik basic auth config: ${TRAEFIK_DYNAMIC}"
  else
    echo "Warning: Could not read GRAFANA_ADMIN_PASSWORD from .env" >&2
  fi
fi

if ! docker network inspect "${NET}" >/dev/null 2>&1; then
  docker network create "${NET}" >/dev/null
  echo "Created Docker network: ${NET}"
else
  echo "Docker network ${NET} already exists"
fi

SERVICEFILE="/etc/systemd/system/${STACK}-docker.service"
if [ ! -f "${SERVICEFILE}" ]; then
  cat > "${SERVICEFILE}" <<EOF
[Unit]
Description=${STACK^} Docker Compose Services (Loki/Grafana/Prometheus)
Requires=docker.service
After=docker.service
Wants=network-online.target
After=network-online.target

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=${BASE}
ExecStart=/usr/bin/docker compose up -d
ExecStop=/usr/bin/docker compose down
TimeoutStartSec=0

[Install]
WantedBy=multi-user.target
EOF
  echo "Created systemd service: ${SERVICEFILE}"
fi

if systemctl is-enabled "${STACK}-docker.service" >/dev/null 2>&1; then
  echo "Service ${STACK}-docker.service is already enabled"
else
  systemctl daemon-reload
  systemctl enable "${STACK}-docker.service"
  echo "Enabled systemd service: ${STACK}-docker.service"
fi

MONITOR_SCRIPT="/usr/local/bin/rosi-monitor"
if [ -f "${SCRIPT_DIR}/monitor.sh" ]; then
  cp "${SCRIPT_DIR}/monitor.sh" "${MONITOR_SCRIPT}"
  chmod +x "${MONITOR_SCRIPT}"
  echo "Installed monitoring script: ${MONITOR_SCRIPT}"
fi

PROM_TARGET_SCRIPT="/usr/local/bin/prometheus-target"
if [ -f "${SCRIPT_DIR}/prometheus-target.sh" ]; then
  cp "${SCRIPT_DIR}/prometheus-target.sh" "${PROM_TARGET_SCRIPT}"
  chmod +x "${PROM_TARGET_SCRIPT}"
  echo "Installed prometheus target helper: ${PROM_TARGET_SCRIPT}"
fi

LOGROTATE_CONFIG="/etc/logrotate.d/rosi-collector"
LOGROTATE_SRC="${CONFIGS_DIR}/logrotate.conf"
if [ -f "${LOGROTATE_SRC}" ]; then
  cp "${LOGROTATE_SRC}" "${LOGROTATE_CONFIG}"
  chmod 644 "${LOGROTATE_CONFIG}"
  echo "Installed logrotate configuration: ${LOGROTATE_CONFIG}"
fi

echo ""
echo "OK. ROSI Collector environment ready:"
echo "  ${BASE}"
echo "  Systemd service: ${STACK}-docker.service"
echo "  Monitor script: ${MONITOR_SCRIPT}"
echo "  Prometheus target helper: ${PROM_TARGET_SCRIPT}"
echo ""
echo "Next steps:"
echo "  cd ${BASE} && docker compose up -d"
echo "  # Or start via systemd: systemctl start ${STACK}-docker.service"
echo "  # Monitor: rosi-monitor status"
echo ""
echo "Note: To update config files, re-run this script - it will copy latest configs"

