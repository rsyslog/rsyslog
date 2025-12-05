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
                INSTALL_DIR|TRAEFIK_DOMAIN|TRAEFIK_EMAIL)
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

# ==========================================
# Install node_exporter on the server itself
# ==========================================
install_node_exporter() {
  local NODE_EXPORTER_VERSION="${NODE_EXPORTER_VERSION:-1.8.2}"
  local ARCH
  
  # Detect architecture
  case "$(uname -m)" in
    x86_64)  ARCH="amd64" ;;
    aarch64) ARCH="arm64" ;;
    armv7l)  ARCH="armv7" ;;
    *)       echo "Unsupported architecture: $(uname -m)" >&2; return 1 ;;
  esac
  
  # Get the Docker bridge gateway IP (so containers can reach node_exporter)
  # This is the IP that Prometheus inside Docker will use to scrape the host
  local BIND_IP=""
  local DOCKER_NETWORK=""
  local project_name
  project_name=$(basename "${BASE}" | tr '[:upper:]' '[:lower:]' | tr -cd '[:alnum:]_-')
  
  # First, check which network the ROSI containers are actually using
  # This is more reliable than guessing the network name
  local sample_container=""
  for container in prometheus-central "${project_name}-prometheus-1" grafana-central "${project_name}-grafana-1"; do
    if docker inspect "${container}" >/dev/null 2>&1; then
      sample_container="${container}"
      break
    fi
  done
  
  if [ -n "${sample_container}" ]; then
    # Get the network name from the running container
    DOCKER_NETWORK=$(docker inspect "${sample_container}" --format '{{range $k, $v := .NetworkSettings.Networks}}{{$k}}{{end}}' 2>/dev/null | grep -E "rosi|collector" | head -1)
    if [ -n "${DOCKER_NETWORK}" ]; then
      BIND_IP=$(docker network inspect "${DOCKER_NETWORK}" --format '{{range .IPAM.Config}}{{.Gateway}}{{end}}' 2>/dev/null)
      if [ -n "${BIND_IP}" ]; then
        echo "Using Docker bridge gateway: ${BIND_IP} (network: ${DOCKER_NETWORK}, detected from ${sample_container})" >&2
      fi
    fi
  fi
  
  # Fallback: try known network names if no container found yet
  if [ -z "${BIND_IP}" ]; then
    for net_name in "${STACK}-net" "${project_name}_${STACK}-net" "${project_name}-${STACK}-net"; do
      if docker network inspect "${net_name}" >/dev/null 2>&1; then
        BIND_IP=$(docker network inspect "${net_name}" --format '{{range .IPAM.Config}}{{.Gateway}}{{end}}' 2>/dev/null)
        if [ -n "${BIND_IP}" ]; then
          echo "Using Docker bridge gateway: ${BIND_IP} (network: ${net_name})" >&2
          break
        fi
      fi
    done
  fi
  
  # Fallback: try to find the docker0 bridge gateway
  if [ -z "${BIND_IP}" ]; then
    BIND_IP=$(ip -4 addr show docker0 2>/dev/null | grep -oP 'inet \K[0-9.]+' | head -1)
    if [ -n "${BIND_IP}" ]; then
      echo "Using docker0 bridge: ${BIND_IP}" >&2
    fi
  fi
  
  # Final fallback: use private network IP (for non-Docker Prometheus setups)
  if [ -z "${BIND_IP}" ]; then
    BIND_IP=$(ip -4 addr show | grep -oP 'inet \K10\.[0-9.]+|inet \K172\.(1[6-9]|2[0-9]|3[0-1])\.[0-9.]+|inet \K192\.168\.[0-9.]+' | head -1)
    if [ -n "${BIND_IP}" ]; then
      echo "Using private network IP: ${BIND_IP}" >&2
    fi
  fi
  
  # Last resort: first non-localhost IP
  if [ -z "${BIND_IP}" ]; then
    BIND_IP=$(ip -4 addr show | grep -oP 'inet \K[0-9.]+' | grep -v '^127\.' | head -1)
  fi
  BIND_IP="${BIND_IP:-0.0.0.0}"
  
  # Check if binary is installed
  local binary_exists=false
  if command -v node_exporter >/dev/null 2>&1; then
    binary_exists=true
    local installed_version
    installed_version=$(node_exporter --version 2>&1 | head -1 | grep -oP 'version \K[0-9.]+' || echo "unknown")
    echo "node_exporter binary found (version: ${installed_version})" >&2
  fi
  
  # Check if systemd service exists and is running
  local service_configured=false
  local service_running=false
  local actual_bind_ip=""
  if [ -f "/etc/systemd/system/node_exporter.service" ]; then
    service_configured=true
    if systemctl is-active --quiet node_exporter 2>/dev/null; then
      service_running=true
      # Get the ACTUAL bind IP from network state (more reliable than service file)
      # ss -tlnp shows what port the process is actually listening on
      if command -v ss >/dev/null 2>&1; then
        actual_bind_ip=$(ss -tlnp 2>/dev/null | grep ':9100' | grep -oP '[0-9.]+(?=:9100)' | head -1)
      elif command -v netstat >/dev/null 2>&1; then
        actual_bind_ip=$(netstat -tlnp 2>/dev/null | grep ':9100' | grep -oP '[0-9.]+(?=:9100)' | head -1)
      fi
      # Fallback to service file if we couldn't detect actual IP
      if [ -z "${actual_bind_ip}" ]; then
        actual_bind_ip=$(grep -oP 'listen-address=\K[0-9.]+' /etc/systemd/system/node_exporter.service 2>/dev/null || echo "")
      fi
    fi
  fi
  
  # If binary exists and service is running on the correct IP, we're done
  if [ "${binary_exists}" = true ] && [ "${service_running}" = true ]; then
    if [ "${actual_bind_ip}" = "${BIND_IP}" ]; then
      echo "node_exporter is running on ${BIND_IP}:9100 (verified)" >&2
      echo "${BIND_IP}"
      return 0
    else
      echo "node_exporter is listening on ${actual_bind_ip}:9100 but should be on ${BIND_IP}:9100" >&2
      echo "Reconfiguring node_exporter service..." >&2
      # Continue to reconfigure the service
      service_configured=false
    fi
  fi
  
  # Install binary if not present
  if [ "${binary_exists}" = false ]; then
    echo "Installing node_exporter ${NODE_EXPORTER_VERSION} for ${ARCH}..." >&2
    
    local DOWNLOAD_URL="https://github.com/prometheus/node_exporter/releases/download/v${NODE_EXPORTER_VERSION}/node_exporter-${NODE_EXPORTER_VERSION}.linux-${ARCH}.tar.gz"
    local TEMP_DIR
    TEMP_DIR=$(mktemp -d)
    
    # Download and extract
    if ! curl -sSL "${DOWNLOAD_URL}" -o "${TEMP_DIR}/node_exporter.tar.gz"; then
      echo "Failed to download node_exporter" >&2
      rm -rf "${TEMP_DIR}"
      return 1
    fi
    
    tar -xzf "${TEMP_DIR}/node_exporter.tar.gz" -C "${TEMP_DIR}"
    
    # Install binary
    install -m 0755 "${TEMP_DIR}/node_exporter-${NODE_EXPORTER_VERSION}.linux-${ARCH}/node_exporter" /usr/local/bin/
    
    # Cleanup
    rm -rf "${TEMP_DIR}"
    
    echo "node_exporter binary installed to /usr/local/bin/" >&2
  fi
  
  # Create node_exporter user if it doesn't exist
  if ! id -u node_exporter >/dev/null 2>&1; then
    useradd --system --no-create-home --shell /usr/sbin/nologin node_exporter
    echo "Created node_exporter system user" >&2
  fi
  
  # Create or update systemd service
  if [ "${service_configured}" = false ] || [ "${service_running}" = false ]; then
    echo "Configuring node_exporter systemd service..." >&2
    
    cat > /etc/systemd/system/node_exporter.service << EOF
[Unit]
Description=Prometheus Node Exporter
Documentation=https://prometheus.io/docs/guides/node-exporter/
Wants=network-online.target
After=network-online.target

[Service]
User=node_exporter
Group=node_exporter
Type=simple
ExecStart=/usr/local/bin/node_exporter \\
    --web.listen-address=${BIND_IP}:9100 \\
    --collector.systemd \\
    --collector.processes

Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
    
    # Enable and start
    systemctl daemon-reload
    systemctl enable --now node_exporter
    
    if systemctl is-active --quiet node_exporter; then
      echo "node_exporter service started on ${BIND_IP}:9100" >&2
    else
      echo "Warning: node_exporter service failed to start" >&2
      systemctl status node_exporter --no-pager 2>&1 | head -10 >&2
    fi
    
    # Configure firewall to allow Docker containers to reach node_exporter
    configure_node_exporter_firewall "${BIND_IP}"
  fi
  
  # Only output the IP to stdout (this is what gets captured)
  echo "${BIND_IP}"
}

# Configure firewall for node_exporter access from Docker containers
configure_node_exporter_firewall() {
  local bind_ip="$1"
  
  # Determine the Docker network subnet from the bind IP
  # e.g., 172.20.0.1 -> 172.20.0.0/16
  local subnet
  subnet=$(echo "${bind_ip}" | sed 's/\.[0-9]*$/\.0\/16/')
  
  # Check if ufw is available and active
  if command -v ufw >/dev/null 2>&1 && ufw status 2>/dev/null | grep -q "Status: active"; then
    # Check if rule already exists
    if ! ufw status | grep -q "${bind_ip} 9100/tcp.*ALLOW.*${subnet}"; then
      echo "Configuring UFW firewall for node_exporter..." >&2
      ufw allow from "${subnet}" to "${bind_ip}" port 9100 proto tcp comment "node_exporter from Docker" >/dev/null 2>&1
      echo "Added UFW rule: allow ${subnet} -> ${bind_ip}:9100" >&2
    else
      echo "UFW rule for node_exporter already exists" >&2
    fi
  # Check for firewalld
  elif command -v firewall-cmd >/dev/null 2>&1 && systemctl is-active --quiet firewalld; then
    # Add rich rule for Docker network access
    local rule="rule family=ipv4 source address=${subnet} destination address=${bind_ip} port port=9100 protocol=tcp accept"
    if ! firewall-cmd --list-rich-rules 2>/dev/null | grep -q "9100"; then
      echo "Configuring firewalld for node_exporter..." >&2
      firewall-cmd --permanent --add-rich-rule="${rule}" >/dev/null 2>&1
      firewall-cmd --reload >/dev/null 2>&1
      echo "Added firewalld rule: allow ${subnet} -> ${bind_ip}:9100" >&2
    else
      echo "firewalld rule for node_exporter already exists" >&2
    fi
  # Check for iptables directly (no firewall manager)
  elif command -v iptables >/dev/null 2>&1; then
    # Check if there's a restrictive INPUT policy
    local policy
    policy=$(iptables -L INPUT -n 2>/dev/null | head -1 | grep -oP 'policy \K\w+')
    if [ "${policy}" = "DROP" ] || [ "${policy}" = "REJECT" ]; then
      # Check if rule exists
      if ! iptables -C INPUT -s "${subnet}" -d "${bind_ip}" -p tcp --dport 9100 -j ACCEPT 2>/dev/null; then
        echo "Configuring iptables for node_exporter..." >&2
        iptables -I INPUT -s "${subnet}" -d "${bind_ip}" -p tcp --dport 9100 -j ACCEPT
        echo "Added iptables rule: allow ${subnet} -> ${bind_ip}:9100" >&2
        echo "Note: This rule is not persistent. Add to /etc/iptables/rules.v4 for persistence." >&2
      fi
    fi
  fi
}

# Add server to prometheus targets
add_server_to_prometheus_targets() {
  local server_ip="$1"
  local targets_file="${BASE}/prometheus-targets/nodes.yml"
  local hostname
  
  hostname=$(hostname -s 2>/dev/null || echo "rosi-collector")
  
  # Check if server is already in targets (by IP or hostname)
  if grep -q "${server_ip}:9100" "${targets_file}" 2>/dev/null; then
    echo "Server ${server_ip} already in Prometheus targets"
    return 0
  fi
  
  # Check if this hostname already exists (IP may have changed)
  if grep -q "host: ${hostname}" "${targets_file}" 2>/dev/null; then
    echo "Server ${hostname} already in Prometheus targets (updating IP to ${server_ip})"
    # Use prometheus-target script to update if available
    if [ -x "${SCRIPT_DIR}/prometheus-target.sh" ]; then
      # Remove old entry and add new one
      "${SCRIPT_DIR}/prometheus-target.sh" remove "${hostname}" 2>/dev/null || true
      "${SCRIPT_DIR}/prometheus-target.sh" add "${server_ip}:9100" \
        "host=${hostname}" "role=monitoring" "network=internal"
    else
      echo "Warning: Cannot update IP without prometheus-target.sh script" >&2
      echo "Please manually update ${targets_file}" >&2
    fi
    return 0
  fi
  
  echo "Adding server to Prometheus targets..."
  
  # Use prometheus-target script if available, otherwise append directly
  if [ -x "${SCRIPT_DIR}/prometheus-target.sh" ]; then
    # Use the helper script (it handles YAML formatting properly)
    "${SCRIPT_DIR}/prometheus-target.sh" add "${server_ip}:9100" \
      "host=${hostname}" "role=monitoring" "network=internal"
  else
    # Fallback: append directly to YAML file
    cat >> "${targets_file}" << EOF

# ROSI Collector Server (auto-added by init.sh)
- targets:
    - ${server_ip}:9100
  labels:
    host: ${hostname}
    role: monitoring
    network: internal
EOF
  fi
  
  echo "Added ${hostname} (${server_ip}:9100) to Prometheus targets"
}

# ==========================================
# Configure server to forward its own syslog
# ==========================================
configure_server_syslog_forwarding() {
  local config_file="/etc/rsyslog.d/99-forward-to-rosi.conf"
  local spool_dir="/var/spool/rsyslog"
  local hostname
  
  hostname=$(hostname -s 2>/dev/null || echo "rosi-collector")
  
  # Non-interactive mode: skip unless explicitly enabled
  if [ ! -t 0 ]; then
    if [ "${SERVER_SYSLOG_FORWARDING:-false}" = "true" ]; then
      echo "Non-interactive: SERVER_SYSLOG_FORWARDING=true, configuring..."
    else
      echo "Non-interactive: Skipping server syslog forwarding (set SERVER_SYSLOG_FORWARDING=true to enable)"
      return 0
    fi
  fi
  
  echo ""
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ Server Syslog Forwarding                                    │"
  echo "│ Forward this server's logs to ROSI Collector (localhost)    │"
  echo "│ This makes server logs visible in Grafana dashboards        │"
  echo "└─────────────────────────────────────────────────────────────┘"
  
  # Check for existing config (interactive only)
  if [ -t 0 ] && [ -f "${config_file}" ]; then
    echo "Existing configuration found: ${config_file}"
    echo "Current configuration:"
    grep -E "target=|port=" "${config_file}" 2>/dev/null | head -3 | sed 's/^/  /'
    echo ""
    read -rp "Overwrite existing configuration? [y/N]: " overwrite
    if [[ ! "${overwrite}" =~ ^[Yy]$ ]]; then
      echo "Keeping existing syslog forwarding configuration"
      return 0
    fi
  fi
  
  if [ -t 0 ]; then
    read -rp "Configure server to forward its syslog to ROSI Collector? [Y/n]: " confirm
    if [[ "${confirm}" =~ ^[Nn]$ ]]; then
      echo "Skipping server syslog forwarding configuration"
      return 0
    fi
  fi
  
  echo "Installing rsyslog forwarding configuration..."
  
  # Create spool directory
  if [ ! -d "${spool_dir}" ]; then
    mkdir -p "${spool_dir}"
    chmod 755 "${spool_dir}"
    echo "Created spool directory: ${spool_dir}"
  fi
  
  # Create forwarding config (same format as client config)
  cat > "${config_file}" << 'EOF'
# ROSI Collector Server - Forward local syslog to collector
# Auto-generated by init.sh
# This forwards the server's own logs to the ROSI Collector stack

# Disk-assisted queue for resilience
$WorkDirectory /var/spool/rsyslog

# Forward all logs to local ROSI Collector
*.* action(
    type="omfwd"
    target="127.0.0.1"
    port="10514"
    protocol="tcp"
    queue.type="LinkedList"
    queue.filename="fwd_rosi"
    queue.maxdiskspace="500m"
    queue.saveonshutdown="on"
    action.resumeRetryCount="-1"
    action.resumeInterval="5"
)
EOF
  
  chmod 644 "${config_file}"
  echo "Created: ${config_file}"
  
  # Note: rsyslog restart happens after all configs are installed
  
  echo ""
  echo "Server syslog forwarding configured!"
  echo "  Config: ${config_file}"
  echo "  Target: 127.0.0.1:10514"
  echo "  Host will appear in Grafana as: ${hostname}"
}

# ==========================================
# Configure impstats for server rsyslog monitoring
# ==========================================
configure_server_impstats() {
  local config_file="/etc/rsyslog.d/10-impstats.conf"
  local interval="${SERVER_IMPSTATS_INTERVAL:-60}"
  
  # Non-interactive mode: skip unless explicitly enabled
  if [ ! -t 0 ]; then
    if [ "${SERVER_IMPSTATS_ENABLED:-false}" = "true" ]; then
      echo "Non-interactive: SERVER_IMPSTATS_ENABLED=true, configuring with interval=${interval}..."
    else
      echo "Non-interactive: Skipping impstats configuration (set SERVER_IMPSTATS_ENABLED=true to enable)"
      return 0
    fi
  fi
  
  echo ""
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ rsyslog Performance Monitoring (impstats)                   │"
  echo "│ Enables metrics: queue depth, message rates, memory usage   │"
  echo "│ Metrics are forwarded to ROSI Collector and visible in      │"
  echo "│ Grafana 'rsyslog Client Health' dashboard                   │"
  echo "└─────────────────────────────────────────────────────────────┘"
  
  # Check for existing config (interactive only)
  if [ -t 0 ] && [ -f "${config_file}" ]; then
    echo "Existing impstats configuration found: ${config_file}"
    echo "Current interval:"
    grep -E "interval=" "${config_file}" 2>/dev/null | head -1 | sed 's/^/  /'
    echo ""
    read -rp "Overwrite existing configuration? [y/N]: " overwrite
    if [[ ! "${overwrite}" =~ ^[Yy]$ ]]; then
      echo "Keeping existing impstats configuration"
      return 0
    fi
  fi
  
  if [ -t 0 ]; then
    read -rp "Enable rsyslog performance metrics (impstats)? [Y/n]: " confirm
    if [[ "${confirm}" =~ ^[Nn]$ ]]; then
      echo "Skipping impstats configuration"
      return 0
    fi
    
    # Ask for reporting interval (interactive only)
    echo ""
    echo "Reporting interval determines how often metrics are collected."
    echo "  60 seconds  - Standard monitoring (recommended)"
    echo "  30 seconds  - More granular monitoring"
    echo "  10 seconds  - High-frequency monitoring (debugging)"
    read -rp "Reporting interval in seconds [60]: " user_interval
    interval="${user_interval:-60}"
    
    # Validate interval is a number
    if ! [[ "${interval}" =~ ^[0-9]+$ ]]; then
      echo "Invalid interval, using default: 60"
      interval="60"
    fi
  fi
  
  echo "Installing impstats configuration..."
  
  cat > "${config_file}" << EOF
# /etc/rsyslog.d/10-impstats.conf
# Enable rsyslog internal statistics reporting
# Auto-generated by init.sh
#
# Metrics include:
# - Queue depth and full events
# - Messages forwarded (count, bytes)
# - Action failures and suspensions
# - Resource usage (memory, file descriptors)

module(load="impstats"
       interval="${interval}"
       severity="7"
       log.syslog="on"
       log.file="off"
       format="json"
       resetCounters="off")
EOF
  
  chmod 644 "${config_file}"
  echo "Created: ${config_file}"
  
  # Note: rsyslog restart happens after both configs are installed
  echo ""
  echo "impstats configured!"
  echo "  Config: ${config_file}"
  echo "  Interval: ${interval} seconds"
  echo "  View metrics in: Grafana → rsyslog Client Health dashboard"
}

# NOTE: Server monitoring setup (node_exporter, syslog forwarding, impstats)
# is called later in the script, after all config files are deployed.
# This ensures the Docker network exists when detecting the bind IP.

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
  fi
  
  TRAEFIK_DOMAIN="${TRAEFIK_DOMAIN:-example.com}"
  
  echo "Substituting domain placeholders in download scripts..."
  find "${BASE}/downloads" -type f -name "*.sh" -exec sed -i \
    -e "s|__TRAEFIK_DOMAIN__|${TRAEFIK_DOMAIN}|g" \
    {} \;
  
  find "${BASE}/downloads" -type f -exec chmod 644 {} \;
  find "${BASE}/downloads" -type f -name "*.sh" -exec chmod +x {} \;
  echo "Download files installed"
fi

install -d -m 0750 "${BASE}/traefik/letsencrypt"
chown -R ${STACK}:${STACK} "${BASE}"

find "${BASE}" -type f -name "*.sh" -exec chmod +x {} \;

# ==========================================
# Interactive .env configuration
# ==========================================
prompt_env_config() {
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "  ROSI Collector Configuration"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  echo "No .env file found. Please provide the following configuration:"
  echo ""
  
  # TRAEFIK_DOMAIN (required)
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ TRAEFIK_DOMAIN                                              │"
  echo "│ The domain or IP address for accessing Grafana/Prometheus  │"
  echo "│ Examples: rosi.example.com, 192.168.1.100                   │"
  echo "└─────────────────────────────────────────────────────────────┘"
  while true; do
    read -rp "Enter domain or IP: " TRAEFIK_DOMAIN
    if [ -n "${TRAEFIK_DOMAIN}" ]; then
      break
    fi
    echo "Error: Domain/IP is required"
  done
  echo ""
  
  # TRAEFIK_EMAIL (required for Let's Encrypt)
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ TRAEFIK_EMAIL                                               │"
  echo "│ Email for Let's Encrypt certificate notifications          │"
  echo "│ (Also used for alert notifications if SMTP configured)     │"
  echo "└─────────────────────────────────────────────────────────────┘"
  local default_email="admin@${TRAEFIK_DOMAIN}"
  read -rp "Enter email [${default_email}]: " TRAEFIK_EMAIL
  TRAEFIK_EMAIL="${TRAEFIK_EMAIL:-${default_email}}"
  echo ""
  
  # GRAFANA_ADMIN_PASSWORD (optional - generate if empty)
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ GRAFANA_ADMIN_PASSWORD                                      │"
  echo "│ Password for Grafana admin user                             │"
  echo "│ Leave empty to auto-generate a secure password              │"
  echo "└─────────────────────────────────────────────────────────────┘"
  read -rsp "Enter password (hidden) or press Enter to generate: " GRAFANA_ADMIN_PASSWORD
  echo ""
  if [ -z "${GRAFANA_ADMIN_PASSWORD}" ]; then
    GRAFANA_ADMIN_PASSWORD="$(genpw)"
    echo "  → Generated secure password (will be shown at end of setup)"
    SHOW_GENERATED_PASSWORD=1
  else
    SHOW_GENERATED_PASSWORD=0
  fi
  echo ""
  
  # SYSLOG_TLS_ENABLED
  echo "┌─────────────────────────────────────────────────────────────┐"
  echo "│ TLS/mTLS Configuration                                      │"
  echo "│ Enable encrypted syslog transport on port 6514?             │"
  echo "│ Recommended for production environments                     │"
  echo "└─────────────────────────────────────────────────────────────┘"
  read -rp "Enable TLS for syslog? [y/N]: " tls_choice
  case "${tls_choice}" in
    [Yy]*)
      SYSLOG_TLS_ENABLED="true"
      echo ""
      echo "  TLS Configuration Options:"
      echo ""
      
      # SYSLOG_TLS_HOSTNAME
      local default_tls_hostname="${TRAEFIK_DOMAIN}"
      read -rp "  TLS hostname [${default_tls_hostname}]: " SYSLOG_TLS_HOSTNAME
      SYSLOG_TLS_HOSTNAME="${SYSLOG_TLS_HOSTNAME:-${default_tls_hostname}}"
      
      # Certificate validity periods
      read -rp "  CA certificate validity in days [3650] (10 years): " SYSLOG_TLS_CA_DAYS
      SYSLOG_TLS_CA_DAYS="${SYSLOG_TLS_CA_DAYS:-3650}"
      
      read -rp "  Server certificate validity in days [1825] (5 years): " SYSLOG_TLS_SERVER_DAYS
      SYSLOG_TLS_SERVER_DAYS="${SYSLOG_TLS_SERVER_DAYS:-1825}"
      
      read -rp "  Client certificate validity in days [730] (2 years): " SYSLOG_TLS_CLIENT_DAYS
      SYSLOG_TLS_CLIENT_DAYS="${SYSLOG_TLS_CLIENT_DAYS:-730}"
      
      # SYSLOG_TLS_AUTHMODE
      echo ""
      echo "  Authentication mode:"
      echo "    anon        - TLS encryption only (no client certificates)"
      echo "    x509/certvalid - Require valid client certificates (mTLS)"
      echo "    x509/name   - Require certificates + verify CN/SAN (strictest)"
      read -rp "  Auth mode [anon]: " SYSLOG_TLS_AUTHMODE
      SYSLOG_TLS_AUTHMODE="${SYSLOG_TLS_AUTHMODE:-anon}"
      
      # SYSLOG_TLS_PERMITTED_PEERS (only for x509/name)
      if [ "${SYSLOG_TLS_AUTHMODE}" = "x509/name" ]; then
        local default_peers="*.${SYSLOG_TLS_HOSTNAME}"
        echo ""
        echo "  Permitted peers (comma-separated CN/SAN patterns):"
        read -rp "  Permitted peers [${default_peers}]: " SYSLOG_TLS_PERMITTED_PEERS
        SYSLOG_TLS_PERMITTED_PEERS="${SYSLOG_TLS_PERMITTED_PEERS:-${default_peers}}"
      else
        SYSLOG_TLS_PERMITTED_PEERS=""
      fi
      ;;
    *)
      SYSLOG_TLS_ENABLED="false"
      SYSLOG_TLS_HOSTNAME="${TRAEFIK_DOMAIN}"
      SYSLOG_TLS_CA_DAYS="3650"
      SYSLOG_TLS_SERVER_DAYS="365"
      SYSLOG_TLS_CLIENT_DAYS="365"
      SYSLOG_TLS_AUTHMODE="anon"
      SYSLOG_TLS_PERMITTED_PEERS=""
      ;;
  esac
  echo ""
  
  # Set remaining defaults
  WRITE_JSON_FILE="${WRITE_JSON_FILE:-off}"
  SMTP_ENABLED="${SMTP_ENABLED:-false}"
  SMTP_HOST="${SMTP_HOST:-}"
  SMTP_PORT="${SMTP_PORT:-587}"
  SMTP_USER="${SMTP_USER:-}"
  SMTP_PASSWORD="${SMTP_PASSWORD:-}"
  ALERT_EMAIL_FROM="${ALERT_EMAIL_FROM:-alerts@${TRAEFIK_DOMAIN}}"
  ALERT_EMAIL_TO="${ALERT_EMAIL_TO:-${TRAEFIK_EMAIL}}"
  SMTP_SKIP_VERIFY="${SMTP_SKIP_VERIFY:-false}"
  
  # Export variables for sed substitution
  export TRAEFIK_DOMAIN TRAEFIK_EMAIL GRAFANA_ADMIN_PASSWORD
  export SYSLOG_TLS_ENABLED SYSLOG_TLS_HOSTNAME
  export SYSLOG_TLS_CA_DAYS SYSLOG_TLS_SERVER_DAYS SYSLOG_TLS_CLIENT_DAYS
  export SYSLOG_TLS_AUTHMODE SYSLOG_TLS_PERMITTED_PEERS
  export WRITE_JSON_FILE SMTP_ENABLED SMTP_HOST SMTP_PORT SMTP_USER SMTP_PASSWORD
  export ALERT_EMAIL_FROM ALERT_EMAIL_TO SMTP_SKIP_VERIFY
  export SHOW_GENERATED_PASSWORD
}

if [ ! -f "${ENVFILE}" ]; then
  ENV_TEMPLATE="${CONFIGS_DIR}/.env.template"
  if [ ! -f "${ENV_TEMPLATE}" ]; then
    echo "Error: Template file not found: ${ENV_TEMPLATE}" >&2
    exit 1
  fi
  
  # Interactive configuration if running in terminal
  if [ -t 0 ]; then
    prompt_env_config
  else
    # Non-interactive defaults
    TRAEFIK_DOMAIN="${TRAEFIK_DOMAIN:-example.com}"
    TRAEFIK_EMAIL="${TRAEFIK_EMAIL:-admin@${TRAEFIK_DOMAIN}}"
    GRAFANA_ADMIN_PASSWORD="${GRAFANA_ADMIN_PASSWORD:-$(genpw)}"
    WRITE_JSON_FILE="${WRITE_JSON_FILE:-off}"
    SMTP_ENABLED="${SMTP_ENABLED:-false}"
    SMTP_HOST="${SMTP_HOST:-}"
    SMTP_PORT="${SMTP_PORT:-587}"
    SMTP_USER="${SMTP_USER:-}"
    SMTP_PASSWORD="${SMTP_PASSWORD:-}"
    ALERT_EMAIL_FROM="${ALERT_EMAIL_FROM:-alerts@${TRAEFIK_DOMAIN}}"
    ALERT_EMAIL_TO="${ALERT_EMAIL_TO:-${TRAEFIK_EMAIL}}"
    SMTP_SKIP_VERIFY="${SMTP_SKIP_VERIFY:-false}"
    SYSLOG_TLS_ENABLED="${SYSLOG_TLS_ENABLED:-false}"
    SYSLOG_TLS_HOSTNAME="${SYSLOG_TLS_HOSTNAME:-${TRAEFIK_DOMAIN}}"
    SYSLOG_TLS_CA_DAYS="${SYSLOG_TLS_CA_DAYS:-3650}"
    SYSLOG_TLS_SERVER_DAYS="${SYSLOG_TLS_SERVER_DAYS:-365}"
    SYSLOG_TLS_CLIENT_DAYS="${SYSLOG_TLS_CLIENT_DAYS:-365}"
    SYSLOG_TLS_AUTHMODE="${SYSLOG_TLS_AUTHMODE:-anon}"
    SYSLOG_TLS_PERMITTED_PEERS="${SYSLOG_TLS_PERMITTED_PEERS:-}"
    SHOW_GENERATED_PASSWORD=0
  fi
  
  sed -e "s|__TRAEFIK_DOMAIN__|${TRAEFIK_DOMAIN}|g" \
      -e "s|__TRAEFIK_EMAIL__|${TRAEFIK_EMAIL}|g" \
      -e "s|__GRAFANA_ADMIN_PASSWORD__|${GRAFANA_ADMIN_PASSWORD}|g" \
      -e "s|__WRITE_JSON_FILE__|${WRITE_JSON_FILE}|g" \
      -e "s|__SMTP_ENABLED__|${SMTP_ENABLED}|g" \
      -e "s|__SMTP_HOST__|${SMTP_HOST}|g" \
      -e "s|__SMTP_PORT__|${SMTP_PORT}|g" \
      -e "s|__SMTP_USER__|${SMTP_USER}|g" \
      -e "s|__SMTP_PASSWORD__|${SMTP_PASSWORD}|g" \
      -e "s|__ALERT_EMAIL_FROM__|${ALERT_EMAIL_FROM}|g" \
      -e "s|__ALERT_EMAIL_TO__|${ALERT_EMAIL_TO}|g" \
      -e "s|__SMTP_SKIP_VERIFY__|${SMTP_SKIP_VERIFY}|g" \
      -e "s|__SYSLOG_TLS_ENABLED__|${SYSLOG_TLS_ENABLED}|g" \
      -e "s|__SYSLOG_TLS_HOSTNAME__|${SYSLOG_TLS_HOSTNAME}|g" \
      -e "s|__SYSLOG_TLS_CA_DAYS__|${SYSLOG_TLS_CA_DAYS}|g" \
      -e "s|__SYSLOG_TLS_SERVER_DAYS__|${SYSLOG_TLS_SERVER_DAYS}|g" \
      -e "s|__SYSLOG_TLS_CLIENT_DAYS__|${SYSLOG_TLS_CLIENT_DAYS}|g" \
      -e "s|__SYSLOG_TLS_AUTHMODE__|${SYSLOG_TLS_AUTHMODE}|g" \
      -e "s|__SYSLOG_TLS_PERMITTED_PEERS__|${SYSLOG_TLS_PERMITTED_PEERS}|g" \
      "${ENV_TEMPLATE}" > "${ENVFILE}"
  chmod 600 "${ENVFILE}"
  chown ${STACK}:${STACK} "${ENVFILE}"
  echo "Created .env file"
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

# ==========================================
# TLS Certificate Setup
# ==========================================
# Auto-generate CA and server certificates if TLS is enabled
SYSLOG_TLS_ENABLED=$(grep "^SYSLOG_TLS_ENABLED=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "false")
SYSLOG_TLS_HOSTNAME=$(grep "^SYSLOG_TLS_HOSTNAME=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "")
SYSLOG_TLS_AUTHMODE=$(grep "^SYSLOG_TLS_AUTHMODE=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "anon")
SYSLOG_TLS_PERMITTED_PEERS=$(grep "^SYSLOG_TLS_PERMITTED_PEERS=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "")

if [[ "${SYSLOG_TLS_ENABLED}" == "true" ]]; then
  echo ""
  echo "TLS is enabled. Setting up certificates..."
  
  CERTS_DIR="${BASE}/certs"
  
  # Check if CA needs to be generated
  if [[ ! -f "${CERTS_DIR}/ca.pem" || ! -f "${CERTS_DIR}/server-cert.pem" ]]; then
    if [[ -z "${SYSLOG_TLS_HOSTNAME}" ]]; then
      SYSLOG_TLS_HOSTNAME="${TRAEFIK_DOMAIN:-localhost}"
      echo "Using TRAEFIK_DOMAIN as TLS hostname: ${SYSLOG_TLS_HOSTNAME}"
    fi
    
    # Get certificate validity from .env or use defaults
    CA_DAYS=$(grep "^SYSLOG_TLS_CA_DAYS=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "3650")
    SERVER_DAYS=$(grep "^SYSLOG_TLS_SERVER_DAYS=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo "365")
    
    echo "Generating CA and server certificates..."
    if [[ -x "${SCRIPT_DIR}/generate-ca.sh" ]]; then
      "${SCRIPT_DIR}/generate-ca.sh" \
        --dir "${CERTS_DIR}" \
        --hostname "${SYSLOG_TLS_HOSTNAME}" \
        --ca-days "${CA_DAYS}" \
        --server-days "${SERVER_DAYS}"
    else
      echo "Error: generate-ca.sh not found or not executable" >&2
      echo "TLS setup incomplete. Run generate-ca.sh manually." >&2
    fi
  else
    echo "TLS certificates already exist in ${CERTS_DIR}"
    # Show expiry info
    if command -v openssl >/dev/null 2>&1; then
      CA_EXPIRY=$(openssl x509 -in "${CERTS_DIR}/ca.pem" -noout -enddate 2>/dev/null | cut -d= -f2 || echo "unknown")
      SERVER_EXPIRY=$(openssl x509 -in "${CERTS_DIR}/server-cert.pem" -noout -enddate 2>/dev/null | cut -d= -f2 || echo "unknown")
      echo "  CA expires: ${CA_EXPIRY}"
      echo "  Server cert expires: ${SERVER_EXPIRY}"
    fi
  fi
  
  # Generate TLS rsyslog config from template
  TLS_CONF_TEMPLATE="${SCRIPT_DIR}/../rsyslog.conf/10-tls-input.conf"
  TLS_CONF_OUTPUT="${BASE}/rsyslog.conf/10-tls-input.conf"
  
  if [[ -f "${TLS_CONF_TEMPLATE}" ]]; then
    echo "Generating TLS rsyslog config (authmode: ${SYSLOG_TLS_AUTHMODE})..."
    
    # Build PermittedPeer line for mTLS mode
    PERMITTED_PEERS_LINE=""
    if [[ "${SYSLOG_TLS_AUTHMODE}" == "x509/name" && -n "${SYSLOG_TLS_PERMITTED_PEERS}" ]]; then
      # Convert comma-separated list to rsyslog array format: ["peer1", "peer2"]
      PEERS_ARRAY=$(echo "${SYSLOG_TLS_PERMITTED_PEERS}" | sed 's/,/", "/g')
      PERMITTED_PEERS_LINE="    PermittedPeer=[\"${PEERS_ARRAY}\"]"
      echo "  Permitted peers: ${SYSLOG_TLS_PERMITTED_PEERS}"
    fi
    
    # Apply substitutions
    sed -e "s/__SYSLOG_TLS_AUTHMODE__/${SYSLOG_TLS_AUTHMODE}/g" \
        -e "s|__SYSLOG_TLS_PERMITTED_PEERS_LINE__|${PERMITTED_PEERS_LINE}|g" \
        "${TLS_CONF_TEMPLATE}" > "${TLS_CONF_OUTPUT}"
    
    chmod 644 "${TLS_CONF_OUTPUT}"
    echo "  Generated: ${TLS_CONF_OUTPUT}"
  fi
  
  # Create tls-packages directory for secure downloads
  install -d -m 0750 "${BASE}/downloads/tls-packages"
  chown ${STACK}:${STACK} "${BASE}/downloads/tls-packages"
  
  # Copy CA certificate to downloads for easy client access
  if [[ -f "${CERTS_DIR}/ca.pem" ]]; then
    cp "${CERTS_DIR}/ca.pem" "${BASE}/downloads/ca.pem"
    chmod 644 "${BASE}/downloads/ca.pem"
    echo "CA certificate available at: https://${TRAEFIK_DOMAIN:-example.com}/downloads/ca.pem"
  fi
  
  echo ""
  echo "TLS setup complete!"
  echo "  Port 6514 will accept TLS connections"
  echo "  Auth mode: ${SYSLOG_TLS_AUTHMODE}"
  if [[ "${SYSLOG_TLS_AUTHMODE}" == "x509/certvalid" ]]; then
    echo "  mTLS enabled - clients must have valid certificates signed by CA"
  elif [[ "${SYSLOG_TLS_AUTHMODE}" == "x509/name" ]]; then
    echo "  mTLS enabled - clients must have certificates matching PERMITTED_PEERS"
  fi
  echo "  Generate client certs: rosi-generate-client-cert --download CLIENT_NAME"
  
  # Configure firewall for TLS port if ufw is available
  if command -v ufw >/dev/null 2>&1; then
    if ufw status | grep -q "Status: active"; then
      if ! ufw status | grep -q "6514/tcp"; then
        echo ""
        echo "Configuring firewall for TLS port 6514..."
        ufw allow 6514/tcp comment 'rsyslog TLS' >/dev/null 2>&1
        echo "  Firewall rule added: allow 6514/tcp (rsyslog TLS)"
      fi
    fi
  fi
else
  echo "TLS is disabled (SYSLOG_TLS_ENABLED=${SYSLOG_TLS_ENABLED})"
fi

# Note: Docker network is created automatically by Docker Compose with proper labels.
# Manual pre-creation would conflict with Compose's network management.
if docker network inspect "${NET}" >/dev/null 2>&1; then
  echo "Docker network ${NET} already exists"
fi

# Set COMPOSE_PROFILES for systemd service based on TLS setting
if [[ "${SYSLOG_TLS_ENABLED}" == "true" ]]; then
  COMPOSE_PROFILES_LINE="Environment=COMPOSE_PROFILES=tls"
  TLS_STATUS="TLS syslog enabled (port 6514)"
else
  COMPOSE_PROFILES_LINE="# TLS disabled"
  TLS_STATUS=""
fi

SERVICEFILE="/etc/systemd/system/${STACK}-docker.service"
# Always regenerate service file to pick up TLS changes
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
${COMPOSE_PROFILES_LINE}
ExecStart=/usr/bin/docker compose up -d
ExecStop=/usr/bin/docker compose down
TimeoutStartSec=0

[Install]
WantedBy=multi-user.target
EOF
echo "Created/updated systemd service: ${SERVICEFILE}"
if [[ -n "${TLS_STATUS}" ]]; then
  echo "  ${TLS_STATUS}"
fi

systemctl daemon-reload

if systemctl is-enabled "${STACK}-docker.service" >/dev/null 2>&1; then
  echo "Service ${STACK}-docker.service is already enabled"
else
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

# Install TLS certificate management scripts
GENERATE_CA_SCRIPT="/usr/local/bin/rosi-generate-ca"
if [ -f "${SCRIPT_DIR}/generate-ca.sh" ]; then
  cp "${SCRIPT_DIR}/generate-ca.sh" "${GENERATE_CA_SCRIPT}"
  chmod +x "${GENERATE_CA_SCRIPT}"
  echo "Installed CA generation script: ${GENERATE_CA_SCRIPT}"
fi

GENERATE_CLIENT_CERT_SCRIPT="/usr/local/bin/rosi-generate-client-cert"
if [ -f "${SCRIPT_DIR}/generate-client-cert.sh" ]; then
  cp "${SCRIPT_DIR}/generate-client-cert.sh" "${GENERATE_CLIENT_CERT_SCRIPT}"
  chmod +x "${GENERATE_CLIENT_CERT_SCRIPT}"
  echo "Installed client cert script: ${GENERATE_CLIENT_CERT_SCRIPT}"
fi

LOGROTATE_CONFIG="/etc/logrotate.d/rosi-collector"
LOGROTATE_SRC="${CONFIGS_DIR}/logrotate.conf"
if [ -f "${LOGROTATE_SRC}" ]; then
  cp "${LOGROTATE_SRC}" "${LOGROTATE_CONFIG}"
  chmod 644 "${LOGROTATE_CONFIG}"
  echo "Installed logrotate configuration: ${LOGROTATE_CONFIG}"
fi

# ==========================================
# Server monitoring setup (node_exporter, syslog forwarding, impstats)
# Done at the end so Docker network is available for IP detection
# ==========================================

# Install node_exporter and add server as first target
echo ""
echo "Setting up server monitoring..."
SERVER_IP=$(install_node_exporter)
if [ -n "${SERVER_IP}" ] && [ "${SERVER_IP}" != "0.0.0.0" ]; then
  add_server_to_prometheus_targets "${SERVER_IP}"
else
  echo "Warning: Could not determine server IP for Prometheus target" >&2
  echo "Add manually with: prometheus-target add <IP>:9100 host=$(hostname -s) role=monitoring" >&2
fi

# Configure server syslog forwarding to ROSI Collector
configure_server_syslog_forwarding

# Configure impstats for rsyslog performance monitoring
configure_server_impstats

# Restart rsyslog if any server config was modified
restart_server_rsyslog() {
  # Check if any server rsyslog config exists
  if [ -f "/etc/rsyslog.d/99-forward-to-rosi.conf" ] || [ -f "/etc/rsyslog.d/10-impstats.conf" ]; then
    echo ""
    echo "Testing rsyslog configuration..."
    if rsyslogd -N1 2>&1 | grep -qi "error"; then
      echo "Warning: rsyslog configuration test reported errors" >&2
      rsyslogd -N1 2>&1 | grep -i error | head -5 >&2
    else
      echo "Configuration test passed"
    fi
    
    echo "Restarting rsyslog service..."
    if systemctl restart rsyslog 2>/dev/null; then
      echo "rsyslog service restarted successfully"
    else
      echo "Warning: Failed to restart rsyslog" >&2
      echo "Restart manually: systemctl restart rsyslog" >&2
    fi
  fi
}
restart_server_rsyslog

echo ""
echo "OK. ROSI Collector environment ready:"
echo "  ${BASE}"
echo "  Systemd service: ${STACK}-docker.service"
echo "  Monitor script: ${MONITOR_SCRIPT}"
echo "  Prometheus target helper: ${PROM_TARGET_SCRIPT}"
echo "  node_exporter: installed and running on this server"
if [[ "${SYSLOG_TLS_ENABLED}" == "true" ]]; then
  echo "  TLS scripts: ${GENERATE_CA_SCRIPT}, ${GENERATE_CLIENT_CERT_SCRIPT}"
  echo "  TLS: enabled (port 6514, authmode: ${SYSLOG_TLS_AUTHMODE})"
fi

# Show generated password if it was auto-generated during interactive setup
if [ "${SHOW_GENERATED_PASSWORD:-0}" = "1" ] && [ -n "${GRAFANA_ADMIN_PASSWORD:-}" ]; then
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "  IMPORTANT: Save your credentials!"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  echo "  Grafana URL:    https://${TRAEFIK_DOMAIN}/"
  echo "  Username:       admin"
  echo "  Password:       ${GRAFANA_ADMIN_PASSWORD}"
  echo ""
  echo "  (Password is also stored in ${ENVFILE})"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
fi

echo ""
echo "Next steps:"
echo "  cd ${BASE} && docker compose up -d"
echo "  # Or start via systemd: systemctl start ${STACK}-docker.service"
echo "  # Monitor: rosi-monitor status"
echo "  # Check node_exporter: systemctl status node_exporter"
echo ""
echo "Note: To update config files, re-run this script - it will copy latest configs"

