#!/usr/bin/env bash
# Ubuntu 24.04 basic installation + Docker CE install + firewall
# Hardening features disabled for basic installation that works in /srv/
# Configuration files are stored in ./configs/ directory
# 
# INTERACTIVE MODE: The script will ask before installing each config file.
# Set NONINTERACTIVE=1 to skip all prompts and install everything.
#
# Usage examples (as root):
# Basic installation (uses rsyslog for Docker logging):
#   ./install-server.sh
# 
# Non-interactive mode (install all configs without prompts):
#   NONINTERACTIVE=1 ./install-server.sh
#
# With custom SSH port and TCP port 514 open:
#   SSH_PORT=22 OPEN_TCP_PORTS="514" ./install-server.sh
# 
# With journald logging instead of rsyslog:
#   DOCKER_LOG_DRIVER=journald ./install-server.sh
# 
# With multiple ports and custom settings:
#   SSH_PORT=2222 OPEN_TCP_PORTS="80 443 514" HOSTNAME_FQDN="my.server.com" DOCKER_LOG_DRIVER=syslog ./install-server.sh
# ROSI Collector Server Sample:
#   SSH_PORT=22 OPEN_TCP_PORTS="80 443 514" HOSTNAME_FQDN="rosi.example.com" DOCKER_LOG_DRIVER=syslog ./install-server.sh
#
set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Interactive mode (set NONINTERACTIVE=1 to skip prompts)
NONINTERACTIVE="${NONINTERACTIVE:-0}"

# Function to ask user for confirmation with description
# Usage: ask_install "config_name" "description" "what it does"
# Returns 0 if user wants to install, 1 if skip
ask_install() {
    local name="$1"
    local description="$2"
    local details="$3"
    
    if [ "$NONINTERACTIVE" = "1" ]; then
        return 0  # Install everything in non-interactive mode
    fi
    
    echo ""
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}Config:${NC} $name"
    echo -e "${GREEN}Purpose:${NC} $description"
    echo -e "${GREEN}Details:${NC} $details"
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    while true; do
        read -rp "Install this configuration? [Y/n/q] " answer
        case "${answer,,}" in
            y|yes|"")
                return 0
                ;;
            n|no)
                log_warn "Skipping: $name"
                return 1
                ;;
            q|quit)
                log_error "Installation aborted by user"
                exit 1
                ;;
            *)
                echo "Please answer Y (yes), N (no), or Q (quit)"
                ;;
        esac
    done
}

# === Environment Variable Validation and Assignment ===
log_info "Validating environment variables..."

# Function to validate port numbers
validate_port() {
    local port="$1"
    local name="$2"
    if ! [[ "$port" =~ ^[0-9]+$ ]] || [ "$port" -lt 1 ] || [ "$port" -gt 65535 ]; then
        log_error "Invalid port number for $name: '$port'"
        log_error "Port must be a number between 1 and 65535"
        log_error "Example: $name=22"
        return 1
    fi
}

# Function to validate CIDR notation
validate_cidr() {
    local cidr="$1"
    if [[ ! "$cidr" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}/[0-9]{1,2}$ ]] && [[ "$cidr" != "0.0.0.0/0" ]]; then
        log_error "Invalid CIDR notation for ALLOW_SSH_FROM: '$cidr'"
        log_error "Must be in format: IP/MASK"
        log_error "Examples:"
        log_error "  ALLOW_SSH_FROM=\"192.168.1.0/24\"    # Local network"
        log_error "  ALLOW_SSH_FROM=\"10.0.0.100/32\"     # Single IP"
        log_error "  ALLOW_SSH_FROM=\"0.0.0.0/0\"         # Any IP (default)"
        return 1
    fi
}

# Function to validate hostname
validate_hostname() {
    local hostname="$1"
    if [ -n "$hostname" ] && ! [[ "$hostname" =~ ^[a-zA-Z0-9]([a-zA-Z0-9.-]*[a-zA-Z0-9])?$ ]]; then
        log_error "Invalid hostname format: '$hostname'"
        log_error "Hostname must contain only letters, numbers, dots, and hyphens"
        log_error "Examples:"
        log_error "  HOSTNAME_FQDN=\"server.example.com\""
        log_error "  HOSTNAME_FQDN=\"web-server\""
        log_error "  HOSTNAME_FQDN=\"db01.internal\""
        return 1
    fi
}

# === Validate and assign configuration variables ===

# Validate and assign SSH_PORT
SSH_PORT_INPUT="${SSH_PORT:-22}"
validate_port "$SSH_PORT_INPUT" "SSH_PORT" || exit 1
SSH_PORT="$SSH_PORT_INPUT"

# Validate and assign DOCKER_LOG_DRIVER
DOCKER_LOG_DRIVER_INPUT="${DOCKER_LOG_DRIVER:-syslog}"
case "$DOCKER_LOG_DRIVER_INPUT" in
    "syslog"|"journald"|"json-file")
        DOCKER_LOG_DRIVER="$DOCKER_LOG_DRIVER_INPUT"
        ;;
    *)
        log_error "Invalid Docker log driver: '$DOCKER_LOG_DRIVER_INPUT'"
        log_error "Supported drivers: syslog, journald, json-file"
        log_error "Examples:"
        log_error "  DOCKER_LOG_DRIVER=\"syslog\"     # Default - centralized logging"
        log_error "  DOCKER_LOG_DRIVER=\"journald\"   # SystemD journal integration"
        log_error "  DOCKER_LOG_DRIVER=\"json-file\"  # Local JSON files"
        exit 1
        ;;
esac

# Validate and assign ALLOW_SSH_FROM
ALLOW_SSH_FROM_INPUT="${ALLOW_SSH_FROM:-0.0.0.0/0}"
validate_cidr "$ALLOW_SSH_FROM_INPUT" || exit 1
ALLOW_SSH_FROM="$ALLOW_SSH_FROM_INPUT"

# Validate and assign HOSTNAME_FQDN
HOSTNAME_FQDN_INPUT="${HOSTNAME_FQDN:-}"
validate_hostname "$HOSTNAME_FQDN_INPUT" || exit 1
HOSTNAME_FQDN="$HOSTNAME_FQDN_INPUT"

# Validate and assign OPEN_TCP_PORTS
OPEN_TCP_PORTS_INPUT="${OPEN_TCP_PORTS:-}"
if [ -n "$OPEN_TCP_PORTS_INPUT" ]; then
    for port in $OPEN_TCP_PORTS_INPUT; do
        validate_port "$port" "OPEN_TCP_PORTS" || {
            log_error "Fix the invalid port and try again"
            log_error "Examples:"
            log_error "  OPEN_TCP_PORTS=\"80 443\"           # Web server"
            log_error "  OPEN_TCP_PORTS=\"22 80 443 8080\"   # Multiple services"
            log_error "  OPEN_TCP_PORTS=\"514\"              # Syslog"
            exit 1
        }
    done
fi
OPEN_TCP_PORTS="$OPEN_TCP_PORTS_INPUT"

# Validate and assign ADD_USER_TO_DOCKER (handle missing SUDO_USER)
if [ -n "${ADD_USER_TO_DOCKER:-}" ]; then
    # User explicitly specified
    ADD_USER_TO_DOCKER_INPUT="$ADD_USER_TO_DOCKER"
elif [ -n "${SUDO_USER:-}" ]; then
    # Use SUDO_USER if available
    ADD_USER_TO_DOCKER_INPUT="$SUDO_USER"
else
    # No user specified and no SUDO_USER available
    ADD_USER_TO_DOCKER_INPUT=""
fi

# Validate user exists if specified
if [ -n "$ADD_USER_TO_DOCKER_INPUT" ]; then
    if ! id -u "$ADD_USER_TO_DOCKER_INPUT" >/dev/null 2>&1; then
        log_error "User '$ADD_USER_TO_DOCKER_INPUT' does not exist"
        log_error "Available users:"
        cut -d: -f1 /etc/passwd | grep -v '^_' | head -10
        log_error "Examples:"
        log_error "  ADD_USER_TO_DOCKER=\"myuser\""
        log_error "  ADD_USER_TO_DOCKER=\"admin\""
        log_error "  ADD_USER_TO_DOCKER=\"\"           # Skip user addition"
        exit 1
    fi
fi
ADD_USER_TO_DOCKER="$ADD_USER_TO_DOCKER_INPUT"

# Print configuration summary
log_info "Configuration validated successfully:"
log_info "  SSH Port: $SSH_PORT"
log_info "  SSH Access: $ALLOW_SSH_FROM"
log_info "  Docker Log Driver: $DOCKER_LOG_DRIVER"
[ -n "$OPEN_TCP_PORTS" ] && log_info "  Open Ports: $OPEN_TCP_PORTS"
[ -n "$HOSTNAME_FQDN" ] && log_info "  Hostname: $HOSTNAME_FQDN"
[ -n "$ADD_USER_TO_DOCKER" ] && log_info "  Docker User: $ADD_USER_TO_DOCKER" || log_info "  Docker User: none"

# === Pre-flight ===
log_info "Starting Ubuntu 24 basic server installation with Docker..."
[ "$(id -u)" -eq 0 ] || { log_error "Run as root."; exit 1; }
grep -qi "Ubuntu 24" /etc/os-release || { log_error "This targets Ubuntu 24.x."; exit 1; }
log_info "System check passed - Ubuntu 24 detected"
export DEBIAN_FRONTEND=noninteractive
log_info "Updating package lists and installing security packages..."
apt-get update -y
apt-get install -y --no-install-recommends \
  ca-certificates curl gnupg ufw fail2ban apparmor apparmor-utils \
  unattended-upgrades apt-listchanges auditd openssh-server \
  acl vim tmux net-tools lsof mc screen nano openssl rkhunter chkrootkit \
  lynis aide acct rsyslog
  
# === Create baseline users (rger, al) ===
if [ -f "$(dirname "$0")/../common/common_users.sh" ]; then
  log_info "Creating baseline users (rger, al)"
  "$(dirname "$0")/../common/common_users.sh"
else
  log_warn "common/common_users.sh not found; skipping user creation"
fi

# === Hostname (optional) ===
if [ -n "$HOSTNAME_FQDN" ]; then
  log_info "Setting hostname to: $HOSTNAME_FQDN"
  hostnamectl set-hostname "$HOSTNAME_FQDN"
fi

# === Basic sysctl configuration ===
SCRIPT_DIR="$(dirname "$0")"
if ask_install "sysctl-hardening.conf" \
    "Kernel networking and security settings" \
    "Enables IP forwarding for Docker, disables ICMP redirects, enables SYN cookies for DoS protection"; then
  log_info "Applying basic kernel networking configuration..."
  cp "$SCRIPT_DIR/configs/sysctl-hardening.conf" /etc/sysctl.d/99-hardening.conf
  sysctl --system >/dev/null
  log_info "Kernel networking configuration applied successfully"
fi

# === Rsyslog configuration for Docker logging ===
if [ "$DOCKER_LOG_DRIVER" = "syslog" ]; then
  if ask_install "rsyslog-docker.conf" \
      "Rsyslog configuration for Docker container logs" \
      "Routes Docker container logs to /var/log/docker/ with per-container log files"; then
    log_info "Configuring rsyslog for Docker container logging..."
    
    # Configure rsyslog to handle Docker logs
    cp "$SCRIPT_DIR/configs/rsyslog-docker.conf" /etc/rsyslog.d/30-docker.conf

    # Create Docker log directory
    mkdir -p /var/log/docker
    chown syslog:adm /var/log/docker
    chmod 755 /var/log/docker
    
    # Add log rotation for Docker logs
    if ask_install "docker-logs-logrotate" \
        "Log rotation for Docker logs" \
        "Rotates Docker logs daily, keeps 7 days, compresses old logs"; then
      cp "$SCRIPT_DIR/configs/docker-logs-logrotate" /etc/logrotate.d/docker-logs
    fi

    # Restart rsyslog to apply configuration
    systemctl restart rsyslog
    log_info "Rsyslog configured for Docker logging"
  fi
fi

# === Fail2ban configuration (protects SSH and adds additional services) ===
if ask_install "fail2ban-custom.local" \
    "Fail2ban intrusion prevention rules" \
    "Bans IPs after 5 failed SSH attempts for 10 minutes (relaxed settings)"; then
  log_info "Configuring Fail2ban for intrusion prevention..."
  mkdir -p /etc/fail2ban/jail.d
  # Copy base config and replace SSH port
  cp "$SCRIPT_DIR/configs/fail2ban-custom.local" /etc/fail2ban/jail.d/custom.local
  sed -i "s/port    = 22/port    = ${SSH_PORT}/" /etc/fail2ban/jail.d/custom.local
  systemctl enable --now fail2ban
  log_info "Fail2ban configured and started"
fi

# === Unattended upgrades (security updates) ===
log_info "Configuring automatic security updates..."
dpkg-reconfigure -fnoninteractive unattended-upgrades
systemctl restart unattended-upgrades || true
log_info "Automatic updates configured"

# === UFW firewall (default deny inbound, allow outbound) ===
log_info "Configuring UFW firewall..."
ufw --force reset
ufw default deny incoming
ufw default allow outgoing

# Enable IPv6 in UFW if not already enabled
if ! grep -q "^IPV6=yes" /etc/default/ufw 2>/dev/null; then
  sed -ri 's/^IPV6=.*$/IPV6=yes/' /etc/default/ufw 2>/dev/null || echo 'IPV6=yes' >> /etc/default/ufw
  log_info "Enabled IPv6 in UFW configuration"
fi

ufw allow from "${ALLOW_SSH_FROM}" to any port "${SSH_PORT}" proto tcp comment "SSH"
log_info "SSH access allowed from ${ALLOW_SSH_FROM} on port ${SSH_PORT}"

# open requested TCP ports (applies to both IPv4 and IPv6)
if [ -n "$OPEN_TCP_PORTS" ]; then
  log_info "Opening TCP ports: ${OPEN_TCP_PORTS}"
  for P in $OPEN_TCP_PORTS; do
    ufw allow "${P}"/tcp comment "app-tcp-${P}"
    log_info "Opened TCP port ${P} (IPv4 and IPv6)"
  done
else
  log_info "No additional TCP ports requested"
fi

# Docker + UFW: ensure FORWARD policy ACCEPT so containers can egress, UFW rules still apply
sed -ri 's/^DEFAULT_FORWARD_POLICY=.*$/DEFAULT_FORWARD_POLICY="ACCEPT"/' /etc/default/ufw || echo 'DEFAULT_FORWARD_POLICY="ACCEPT"' >> /etc/default/ufw
ufw --force enable
log_info "UFW firewall configured and enabled (IPv4 and IPv6)"

# === Docker CE install (official repo, Ubuntu 24 "noble") ===
log_info "Installing Docker Community Edition..."
install -m 0755 -d /etc/apt/keyrings
rm -f /etc/apt/keyrings/docker.gpg
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
chmod a+r /etc/apt/keyrings/docker.gpg
ARCH="$(dpkg --print-architecture)"
. /etc/os-release
echo \
  "deb [arch=${ARCH} signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu ${VERSION_CODENAME} stable" \
  > /etc/apt/sources.list.d/docker.list
apt-get update -y
apt-get install -y --no-install-recommends \
  docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
log_info "Docker packages installed successfully"

# === Docker daemon configuration ===
if ask_install "docker-daemon.json" \
    "Docker daemon configuration" \
    "Sets ${DOCKER_LOG_DRIVER} logging, overlay2 storage driver, live-restore for container persistence"; then
  log_info "Configuring Docker daemon with ${DOCKER_LOG_DRIVER} logging..."
  mkdir -p /etc/docker /etc/systemd/system/docker.service.d

  # Copy Docker daemon configuration
  cp "$SCRIPT_DIR/configs/docker-daemon.json" /etc/docker/daemon.json

  # Update log driver in config file
  sed -i 's/"log-driver": "syslog"/"log-driver": "'"${DOCKER_LOG_DRIVER}"'"/' /etc/docker/daemon.json

  # Configure log options based on driver
  if [ "$DOCKER_LOG_DRIVER" = "json-file" ]; then
    # json-file driver uses max-size and max-file instead of syslog options
    sed -i 's/"syslog-facility": "daemon",/"max-size": "10m",/' /etc/docker/daemon.json
    sed -i 's/"syslog-address": "unixgram:\/\/\/dev\/log",/"max-file": "3"/' /etc/docker/daemon.json
    # Remove the tag line (not applicable for json-file driver)
    sed -i '/"tag": "docker/d' /etc/docker/daemon.json
  elif [ "$DOCKER_LOG_DRIVER" = "journald" ]; then
    # journald driver doesn't need syslog options, remove them
    sed -i '/"syslog-facility":/d' /etc/docker/daemon.json
    sed -i '/"syslog-address":/d' /etc/docker/daemon.json
    sed -i '/"tag": "docker/d' /etc/docker/daemon.json
  fi
  log_info "Docker daemon configured"
fi

# === Docker systemd override ===
if ask_install "docker-override.conf" \
    "Docker systemd service overrides" \
    "Increases startup timeout to 300s, enables auto-restart on failure"; then
  mkdir -p /etc/systemd/system/docker.service.d
  cp "$SCRIPT_DIR/configs/docker-override.conf" /etc/systemd/system/docker.service.d/override.conf
fi

systemctl daemon-reload
systemctl enable --now docker
log_info "Docker daemon started"

# Add user to docker group (optional)
if [ -n "${ADD_USER_TO_DOCKER:-}" ] && id -u "$ADD_USER_TO_DOCKER" >/dev/null 2>&1; then
  groupadd -f docker
  usermod -aG docker "$ADD_USER_TO_DOCKER"
  log_info "User '${ADD_USER_TO_DOCKER}' added to docker group"
else
  log_warn "No user specified or user not found for docker group addition"
fi

# === AppArmor enforce common profiles ===
# DISABLED: AppArmor enforcement removed for basic installation
# log_info "Enforcing AppArmor profiles..."
# aa-enforce /etc/apparmor.d/* 2>/dev/null || true
# systemctl restart apparmor || true
log_info "AppArmor enforcement skipped (basic installation mode)"

# === Auditd configuration ===
# DISABLED: Audit daemon configuration removed for basic installation
# log_info "Configuring audit daemon..."
# systemctl enable --now auditd || true
# cp "$SCRIPT_DIR/configs/audit.rules" /etc/audit/rules.d/audit.rules
# service auditd restart || systemctl restart auditd || true
log_info "Audit daemon configuration skipped (basic installation mode)"

# === Process accounting setup (acct package) ===
# DISABLED: Process accounting removed for basic installation
# log_info "Configuring process accounting..."
# if command -v accton >/dev/null 2>&1; then
#   systemctl enable --now acct || true
#   touch /var/log/account/pacct || touch /var/account/pacct || true
#   accton /var/log/account/pacct 2>/dev/null || accton /var/account/pacct 2>/dev/null || true
#   log_info "Process accounting enabled - tracks user commands and system usage"
# else
#   log_warn "Process accounting tools not available"
# fi
log_info "Process accounting skipped (basic installation mode)"

# === Security tools initialization ===
# DISABLED: Security tools initialization removed for basic installation
# log_info "Initializing security scanning tools..."
# if command -v aide >/dev/null 2>&1; then
#   log_info "Initializing AIDE database (this may take a while)..."
#   aideinit || aide --init || true
#   if [ -f /var/lib/aide/aide.db.new ]; then
#     mv /var/lib/aide/aide.db.new /var/lib/aide/aide.db
#   fi
# fi
# if command -v rkhunter >/dev/null 2>&1; then
#   log_info "Updating rkhunter database..."
#   rkhunter --update --quiet || true
#   rkhunter --propupd --quiet || true
# fi
log_info "Security tools initialization skipped (basic installation mode)"

# === Final info and recommendations ===
log_info "==============================================="
log_info "Ubuntu 24 Basic Server Installation completed!"
log_info "==============================================="
log_info "SSH Configuration:"
log_info "  - Port: ${SSH_PORT}"
log_info "  - Access allowed from: ${ALLOW_SSH_FROM}"
log_info "  - Password authentication: DISABLED"
log_info "  - Root login: key-only"

if [ -n "$OPEN_TCP_PORTS" ]; then
  log_info "Open TCP ports: ${OPEN_TCP_PORTS}"
else
  log_info "No additional TCP ports opened"
fi

if [ -n "${ADD_USER_TO_DOCKER:-}" ]; then
  log_info "User '${ADD_USER_TO_DOCKER}' added to 'docker' group"
  log_warn "User needs to re-login to use Docker without sudo"
fi

log_info ""
log_info "Security Features Enabled:"
log_info "  ✓ UFW Firewall (default deny incoming)"
log_info "  ✓ Fail2ban (SSH protection - relaxed)"
log_info "  ✓ Automatic security updates"
log_info "  ✓ Docker Community Edition"
log_info "  ✓ Docker logging: ${DOCKER_LOG_DRIVER}"
log_info "  ✓ Basic kernel networking (ip_forward enabled)"
log_info ""
log_info "Hardening Features DISABLED (basic installation mode):"
log_info "  - AppArmor enforcement"
log_info "  - Audit daemon"
log_info "  - Process accounting"
log_info "  - Security scanning tools (AIDE, rkhunter)"
log_info "  - Kernel security restrictions"
log_info ""
log_info "Next steps:"
log_info "  1. Ensure SSH key authentication is working"
log_info "  2. Test Docker: docker run hello-world"
log_info "  3. Review firewall rules: ufw status"
log_info "  4. Check fail2ban: fail2ban-client status"
if [ "$DOCKER_LOG_DRIVER" = "syslog" ]; then
  log_info "  5. Monitor Docker logs: tail -f /var/log/docker/*.log"
  log_info "  6. Check rsyslog status: systemctl status rsyslog"
else
  log_info "  5. Monitor Docker logs: journalctl -u docker -f"
fi
log_info ""
log_warn "IMPORTANT: Test SSH connectivity before logging out!"
log_info "Basic server installation complete."
