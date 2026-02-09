#!/usr/bin/env bash
# install-node-exporter.sh
# Automated installation script for Prometheus Node Exporter
# Automatically fetches latest version from GitHub and configures with local network IP binding
#
# Usage:
#   sudo ./install-node-exporter.sh [--overwrite]
#
# Options:
#   --overwrite   Overwrite existing node_exporter (stop service / free port 9100)
#
set -euo pipefail

OVERWRITE_FLAG=false

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1" >&2; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $1" >&2; }
log_prompt() { echo -e "${BLUE}[PROMPT]${NC} $1" >&2; }

usage() {
    echo "Usage: sudo $0 [--overwrite]" >&2
    echo "  --overwrite   Overwrite existing node_exporter (stop service and reinstall)" >&2
}

need_root() {
    [ "${EUID:-$(id -u)}" -eq 0 ] || {
        log_error "This script must be run as root"
        log_error "Usage: sudo $0"
        exit 1
    }
}

check_required_commands() {
    local missing=()
    
    for cmd in tar systemctl; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "Missing required commands: ${missing[*]}"
        log_error "Please install the required packages and try again"
        exit 1
    fi
    
    if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
        log_error "Either curl or wget is required"
        exit 1
    fi
}

detect_local_ips() {
    local ips=()
    local ip
    
    while IFS= read -r ip; do
        [ -n "$ip" ] && validate_ip "$ip" && ips+=("$ip")
    done < <(ip addr show 2>/dev/null | grep -E "inet (10\.|192\.168\.|172\.(1[6-9]|2[0-9]|3[01])\.)" | awk '{print $2}' | cut -d/ -f1 || true)
    
    printf '%s\n' "${ips[@]}"
}

validate_ip() {
    local ip="$1"
    if [[ $ip =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$ ]]; then
        local a="${BASH_REMATCH[1]}"
        local b="${BASH_REMATCH[2]}"
        local c="${BASH_REMATCH[3]}"
        local d="${BASH_REMATCH[4]}"
        if [ "$a" -le 255 ] && [ "$b" -le 255 ] && [ "$c" -le 255 ] && [ "$d" -le 255 ] && \
           [ "$a" -ge 0 ] && [ "$b" -ge 0 ] && [ "$c" -ge 0 ] && [ "$d" -ge 0 ]; then
            return 0
        fi
    fi
    return 1
}

get_latest_version() {
    local version
    local retries=3
    local attempt=1
    
    log_info "Fetching latest node_exporter version from GitHub..."
    
    if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
        log_error "curl or wget is required to fetch version information"
        exit 1
    fi
    
    while [ $attempt -le $retries ]; do
        if [ $attempt -gt 1 ]; then
            log_info "Attempt $attempt of $retries..."
        fi
        
        if command -v curl >/dev/null 2>&1; then
            version=$(curl -s --connect-timeout 10 --max-time 30 \
                https://api.github.com/repos/prometheus/node_exporter/releases/latest 2>&1 | \
                grep '"tag_name":' | sed -E 's/.*"v([^"]+)".*/\1/' || echo "")
        else
            version=$(wget -qO- --timeout=30 \
                https://api.github.com/repos/prometheus/node_exporter/releases/latest 2>&1 | \
                grep '"tag_name":' | sed -E 's/.*"v([^"]+)".*/\1/' || echo "")
        fi
        
        if [ -n "$version" ] && [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            log_info "Latest version: $version"
            echo "$version"
            return 0
        fi
        
        if [ $attempt -lt $retries ]; then
            log_warn "Attempt $attempt failed, retrying in 2 seconds..."
            sleep 2
        fi
        attempt=$((attempt + 1))
    done
    
    log_warn "Failed to fetch latest version from GitHub after $retries attempts"
    log_warn "This may be due to network connectivity issues"
    log_warn "Falling back to version 1.7.0"
    echo "1.7.0"
}

detect_architecture() {
    local arch
    arch=$(uname -m)
    case "$arch" in
        x86_64)
            echo "amd64"
            ;;
        aarch64|arm64)
            echo "arm64"
            ;;
        armv7l|armv6l)
            echo "armv7"
            ;;
        *)
            log_warn "Unknown architecture: $arch, assuming amd64"
            echo "amd64"
            ;;
    esac
}

detect_all_ips() {
    local ips=()
    local ip
    
    while IFS= read -r ip; do
        [ -n "$ip" ] && validate_ip "$ip" && ips+=("$ip")
    done < <(ip addr show 2>/dev/null | grep "inet " | awk '{print $2}' | cut -d/ -f1 || true)
    
    printf '%s\n' "${ips[@]}"
}

prompt_ip_address() {
    local detected_ips
    local all_ips
    local selected_ip
    local user_input
    
    detected_ips=($(detect_local_ips))
    all_ips=($(detect_all_ips))
    
    if [ ${#detected_ips[@]} -eq 0 ]; then
        log_warn "No local network IPs detected (10.x.x.x, 192.168.x.x, 172.16-31.x.x)"
        if [ ${#all_ips[@]} -gt 0 ]; then
            log_info "Detected IP addresses on this system:"
            for i in "${!all_ips[@]}"; do
                echo "  [$((i+1))] ${all_ips[$i]}" >&2
            done
            echo "" >&2
        fi
        log_prompt "Please enter the IP address to bind node_exporter (can be local or external):"
        read -r user_input < /dev/tty
        while ! validate_ip "$user_input"; do
            log_error "Invalid IP address format: $user_input"
            log_prompt "Please enter a valid IP address (e.g., 10.135.0.10 or external IP):"
            read -r user_input < /dev/tty
        done
        echo "$user_input"
        return
    fi
    
    log_info "Detected IP addresses on this system:"
    local display_count=0
    for i in "${!all_ips[@]}"; do
        display_count=$((display_count + 1))
        local ip_type=""
        if [[ " ${detected_ips[*]} " =~ " ${all_ips[$i]} " ]]; then
            ip_type=" (local)"
        else
            ip_type=" (external)"
        fi
        echo "  [$display_count] ${all_ips[$i]}$ip_type" >&2
    done
    echo "" >&2
    
    if [ ${#all_ips[@]} -eq 1 ]; then
        log_info "Using detected IP: ${all_ips[0]}"
        log_prompt "Press Enter to use ${all_ips[0]} or type a different IP address:"
        read -r user_input < /dev/tty
        if [ -z "$user_input" ]; then
            echo "${all_ips[0]}"
            return
        elif validate_ip "$user_input"; then
            echo "$user_input"
            return
        else
            log_warn "Invalid IP address, using detected IP: ${all_ips[0]}"
            echo "${all_ips[0]}"
            return
        fi
    fi
    
    log_prompt "Select IP address to bind node_exporter [1-${#all_ips[@]}] or enter custom IP:"
    echo -n "" >&2
    read -r user_input < /dev/tty
    
    if [ -z "$user_input" ]; then
        selected_ip="${all_ips[0]}"
    elif [[ "$user_input" =~ ^[0-9]+$ ]] && [ "$user_input" -ge 1 ] && [ "$user_input" -le ${#all_ips[@]} ]; then
        selected_ip="${all_ips[$((user_input-1))]}"
    elif validate_ip "$user_input"; then
        selected_ip="$user_input"
        log_info "Using custom IP: $selected_ip"
    else
        log_warn "Invalid selection, using first detected IP: ${all_ips[0]}"
        selected_ip="${all_ips[0]}"
    fi
    
    echo "$selected_ip"
}

check_port_availability() {
    local bind_ip="$1"
    local port=9100

    if command -v ss >/dev/null 2>&1; then
        if ! ss -tlnp 2>/dev/null | grep -q ":${port}"; then
            return 0
        fi
    elif command -v netstat >/dev/null 2>&1; then
        if ! netstat -tlnp 2>/dev/null | grep -q ":${port}"; then
            return 0
        fi
    else
        return 0
    fi

    log_warn "Port $port is already in use"
    local who
    who=$(is_port_9100_node_exporter 2>/dev/null); who=${who:-other}
    if [ "$who" = "node_exporter" ]; then
        if [ "$OVERWRITE_FLAG" = true ]; then
            log_info "Overwriting existing node_exporter (--overwrite)"
            stop_existing_node_exporter
            return 0
        fi
        log_prompt "Port 9100 is in use by node_exporter. Overwrite existing installation? [y/N]"
        read -r reply < /dev/tty
        if [[ "$reply" =~ ^[Yy]$ ]]; then
            stop_existing_node_exporter
            return 0
        fi
        log_error "Installation cancelled (port 9100 in use)"
        return 1
    fi
    log_error "Port $port is in use by another service"
    return 1
}

# Stop and disable any known node_exporter systemd unit (etc or usr lib).
# Returns 0 if a service was stopped, 1 if none found.
stop_node_exporter_systemd() {
    local stopped=0
    for unit in node_exporter prometheus-node-exporter; do
        if systemctl list-unit-files --full "${unit}.service" 2>/dev/null | grep -q "^${unit}.service"; then
            if systemctl is-active --quiet "$unit" 2>/dev/null; then
                log_info "Stopping existing $unit service..."
                systemctl stop "$unit" || {
                    log_error "Failed to stop $unit"
                    return 1
                }
                stopped=1
            fi
            systemctl disable "$unit" 2>/dev/null || true
        fi
    done
    [ "$stopped" -eq 1 ] && return 0
    return 1
}

# Get PID of process listening on port 9100 (empty if none or unknown).
get_pid_on_port_9100() {
    if command -v ss >/dev/null 2>&1; then
        ss -tlnp 2>/dev/null | grep ":9100" | sed -n 's/.*pid=\([0-9]*\).*/\1/p' | head -1
    elif command -v netstat >/dev/null 2>&1; then
        netstat -tlnp 2>/dev/null | awk '/:9100 / { split($NF, a, "/"); print a[1]; exit }'
    fi
}

# If something is listening on port 9100, echo "node_exporter" or "other".
# Checks the process on port 9100 first (accurate); falls back to systemctl only if PID unknown.
is_port_9100_node_exporter() {
    local pid
    pid=$(get_pid_on_port_9100)
    if [ -n "$pid" ] && [ -d "/proc/$pid" ] 2>/dev/null; then
        local cmd
        cmd=$(readlink -f "/proc/$pid/exe" 2>/dev/null || true)
        if [[ "$cmd" == *"node_exporter"* ]]; then
            echo "node_exporter"
            return 0
        fi
        echo "other"
        return 0
    fi
    # Fallback: no process identified on port 9100, check if node_exporter service is running
    if systemctl is-active --quiet node_exporter 2>/dev/null || systemctl is-active --quiet prometheus-node-exporter 2>/dev/null; then
        echo "node_exporter"
        return 0
    fi
    echo "other"
    return 0
}

# Stop any existing node_exporter (systemd and/or process on 9100) so install can proceed.
stop_existing_node_exporter() {
    stop_node_exporter_systemd || true
    local who
    who=$(is_port_9100_node_exporter 2>/dev/null); who=${who:-other}
    if [ "$who" = "node_exporter" ]; then
        local pid
        pid=$(get_pid_on_port_9100)
        if [ -n "$pid" ] && [ "$pid" -gt 0 ] 2>/dev/null; then
            log_info "Stopping process on port 9100 (PID $pid)..."
            kill "$pid" 2>/dev/null || true
            sleep 1
            if kill -0 "$pid" 2>/dev/null; then
                kill -9 "$pid" 2>/dev/null || true
            fi
        fi
    fi
}

check_existing_installation() {
    if [ -f "/etc/systemd/system/node_exporter.service" ] || [ -f "/usr/lib/systemd/system/node_exporter.service" ]; then
        log_warn "node_exporter service already exists"
        if systemctl is-active --quiet node_exporter 2>/dev/null; then
            log_info "Stopping existing service..."
            systemctl stop node_exporter || {
                log_error "Failed to stop existing service"
                exit 1
            }
        fi
        systemctl disable node_exporter 2>/dev/null || true
        return 0
    fi
    if [ -f "/usr/lib/systemd/system/prometheus-node-exporter.service" ]; then
        log_warn "prometheus-node-exporter service already exists"
        if systemctl is-active --quiet prometheus-node-exporter 2>/dev/null; then
            log_info "Stopping existing service..."
            systemctl stop prometheus-node-exporter || {
                log_error "Failed to stop existing service"
                exit 1
            }
        fi
        systemctl disable prometheus-node-exporter 2>/dev/null || true
        return 0
    fi
    return 1
}

prompt_central_server_ip() {
    local user_input
    
    log_prompt "Enter central server IP address for firewall rule (optional, press Enter to skip):"
    read -r user_input < /dev/tty
    
    if [ -z "$user_input" ]; then
        echo ""
        return
    fi
    
        while ! validate_ip "$user_input"; do
            log_error "Invalid IP address format: $user_input"
            log_prompt "Please enter a valid IP address or press Enter to skip:"
            read -r user_input < /dev/tty
            [ -z "$user_input" ] && break
        done
    
    echo "$user_input"
}

install_node_exporter() {
    local version="$1"
    local arch="$2"
    local bind_ip="$3"
    local download_url
    local temp_dir
    local binary_path="/usr/local/bin/node_exporter"
    
    log_info "Installing node_exporter version $version for architecture $arch..."
    
    download_url="https://github.com/prometheus/node_exporter/releases/download/v${version}/node_exporter-${version}.linux-${arch}.tar.gz"
    temp_dir=$(mktemp -d)
    
    trap "rm -rf '$temp_dir'" EXIT
    
    log_info "Downloading from: $download_url"
    
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "${temp_dir}/node_exporter.tar.gz" "$download_url" || {
            log_error "Failed to download node_exporter"
            exit 1
        }
    else
        wget -O "${temp_dir}/node_exporter.tar.gz" "$download_url" || {
            log_error "Failed to download node_exporter"
            exit 1
        }
    fi
    
    log_info "Extracting archive..."
    tar -xzf "${temp_dir}/node_exporter.tar.gz" -C "$temp_dir" || {
        log_error "Failed to extract archive"
        exit 1
    }
    
    log_info "Installing binary to $binary_path..."
    install -m 755 "${temp_dir}/node_exporter-${version}.linux-${arch}/node_exporter" "$binary_path" || {
        log_error "Failed to install binary"
        exit 1
    }
    
    log_info "Binary installed successfully"
}

create_system_user() {
    if id -u node_exporter >/dev/null 2>&1; then
        log_info "User 'node_exporter' already exists"
    else
        log_info "Creating system user 'node_exporter'..."
        useradd --no-create-home --shell /bin/false node_exporter || {
            log_error "Failed to create user"
            exit 1
        }
        log_info "User created successfully"
    fi
}

create_systemd_service() {
    local bind_ip="$1"
    local service_file="/etc/systemd/system/node_exporter.service"
    
    log_info "Creating systemd service file..."
    
    cat > "$service_file" <<EOF
[Unit]
Description=Prometheus Node Exporter
Documentation=https://github.com/prometheus/node_exporter
After=network.target

[Service]
User=node_exporter
Group=node_exporter
Type=simple
ExecStart=/usr/local/bin/node_exporter \\
    --web.listen-address=${bind_ip}:9100 \\
    --path.procfs=/proc \\
    --path.sysfs=/sys \\
    --collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)(\$|/)
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
    
    log_info "Service file created: $service_file"
}

check_iptables_persistent() {
    if [ ! -f "/etc/iptables/rules.v4" ]; then
        return 1
    fi
    
    if dpkg -l 2>/dev/null | grep -qi "iptables-persistent"; then
        return 0
    fi
    
    if command -v iptables-restore >/dev/null 2>&1 && command -v netfilter-persistent >/dev/null 2>&1; then
        return 0
    fi
    
    if command -v iptables-restore >/dev/null 2>&1; then
        log_warn "iptables rules file exists but iptables-persistent package not detected"
        log_warn "Will attempt to configure iptables anyway"
        return 0
    fi
    
    return 1
}

configure_iptables() {
    local bind_ip="$1"
    local central_ip="$2"
    local rules_file="/etc/iptables/rules.v4"
    
    if ! check_iptables_persistent; then
        return 1
    fi
    
    if ! command -v iptables >/dev/null 2>&1; then
        log_error "iptables command not found"
        return 1
    fi
    
    log_info "iptables-persistent detected with rules file: $rules_file"
    if [ -n "$central_ip" ]; then
        log_prompt "Add iptables rule to allow $central_ip -> $bind_ip:9100? [y/N]"
    else
        log_prompt "Add iptables rule to allow connections to $bind_ip:9100? [y/N]"
    fi
    read -r confirm < /dev/tty
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "Skipping iptables configuration"
        return 0
    fi
    
    local iptables_cmd_check="iptables -C INPUT -d $bind_ip"
    local iptables_cmd_add="iptables -A INPUT -d $bind_ip"
    local rule_desc=""
    
    if [ -n "$central_ip" ]; then
        iptables_cmd_check="$iptables_cmd_check -s $central_ip"
        iptables_cmd_add="$iptables_cmd_add -s $central_ip"
        rule_desc="$central_ip -> $bind_ip:9100"
    else
        rule_desc="$bind_ip:9100 (any source)"
    fi
    
    iptables_cmd_check="$iptables_cmd_check -p tcp --dport 9100 -j ACCEPT"
    iptables_cmd_add="$iptables_cmd_add -p tcp --dport 9100 -j ACCEPT"
    
    log_info "Checking if rule already exists..."
    if eval "$iptables_cmd_check" 2>/dev/null; then
        log_info "iptables rule for $rule_desc already exists"
        return 0
    fi
    
    log_info "Adding iptables rule: $iptables_cmd_add"
    
    local backup_file="${rules_file}.backup.$(date +%Y%m%d_%H%M%S)"
    if [ -f "$rules_file" ]; then
        cp "$rules_file" "$backup_file" || {
            log_error "Failed to create backup of $rules_file"
            return 1
        }
        log_info "Backup created: $backup_file"
    fi
    
    if eval "$iptables_cmd_add" 2>&1; then
        log_info "iptables rule added successfully"
    else
        local add_error=$?
        log_error "Failed to add iptables rule (exit code: $add_error)"
        return 1
    fi
    
    log_info "Saving iptables rules to $rules_file..."
    if command -v iptables-save >/dev/null 2>&1; then
        if iptables-save > "$rules_file" 2>&1; then
            log_info "iptables rules saved successfully"
        else
            local save_error=$?
            log_error "Failed to save iptables rules (exit code: $save_error)"
            log_warn "Rule was added to running iptables but not saved to file"
            if [ -f "$backup_file" ]; then
                log_warn "Backup available at: $backup_file"
            fi
            return 1
        fi
    else
        log_error "iptables-save not found, cannot save rules"
        log_warn "Rule was added to running iptables but not saved to file"
        return 1
    fi
    
    return 0
}

configure_firewall() {
    local bind_ip="$1"
    local central_ip="$2"
    local configured=false
    
    log_info "Configuring firewall rules..."
    if [ -n "$central_ip" ]; then
        log_info "Central server IP: $central_ip"
    else
        log_info "No central server IP provided, allowing all connections to $bind_ip:9100"
    fi
    
    if check_iptables_persistent; then
        if configure_iptables "$bind_ip" "$central_ip"; then
            configured=true
        fi
    fi
    
    if command -v ufw >/dev/null 2>&1; then
        if ufw status 2>/dev/null | grep -q "Status: active"; then
            log_info "Configuring UFW rule..."
            if [ -n "$central_ip" ]; then
                ufw allow from "$central_ip" to any port 9100 proto tcp comment "Prometheus node exporter" 2>/dev/null || {
                    log_warn "Failed to add UFW rule (may already exist)"
                }
            else
                ufw allow 9100/tcp comment "Prometheus node exporter" 2>/dev/null || {
                    log_warn "Failed to add UFW rule (may already exist)"
                }
            fi
            configured=true
        else
            log_warn "UFW is not active, skipping UFW configuration"
        fi
    fi
    
    if [ "$configured" = true ]; then
        log_info "Firewall configuration completed"
    else
        log_warn "No firewall configured (neither iptables-persistent nor active UFW found)"
        if [ -n "$central_ip" ]; then
            log_warn "Please manually configure firewall to allow $central_ip -> $bind_ip:9100"
        else
            log_warn "Please manually configure firewall to allow connections to $bind_ip:9100"
        fi
    fi
}

enable_and_start_service() {
    log_info "Reloading systemd daemon..."
    systemctl daemon-reload || {
        log_error "Failed to reload systemd"
        exit 1
    }
    
    log_info "Enabling node_exporter service..."
    systemctl enable node_exporter || {
        log_error "Failed to enable service"
        exit 1
    }
    
    log_info "Starting node_exporter service..."
    systemctl start node_exporter || {
        log_error "Failed to start service"
        exit 1
    }
    
    sleep 2
    
    if systemctl is-active --quiet node_exporter; then
        log_info "Service started successfully"
    else
        log_error "Service failed to start"
        log_error "Check logs: journalctl -u node_exporter -n 50"
        exit 1
    fi
}

verify_installation() {
    local bind_ip="$1"
    local status
    local listening
    local max_wait=30
    local waited=0
    
    log_info "Verifying installation..."
    
    while [ $waited -lt $max_wait ]; do
        status=$(systemctl is-active node_exporter 2>/dev/null || echo "inactive")
        if [ "$status" = "active" ]; then
            break
        fi
        sleep 1
        waited=$((waited + 1))
    done
    
    if [ "$status" != "active" ]; then
        log_error "Service is not active (status: $status)"
        return 1
    fi
    
    if command -v ss >/dev/null 2>&1; then
        listening=$(ss -tlnp 2>/dev/null | grep ":9100" || echo "")
    elif command -v netstat >/dev/null 2>&1; then
        listening=$(netstat -tlnp 2>/dev/null | grep ":9100" || echo "")
    else
        listening=""
    fi
    
    if [ -z "$listening" ]; then
        log_error "Service is not listening on port 9100"
        return 1
    fi
    
    if ! echo "$listening" | grep -q "${bind_ip}:9100"; then
        log_warn "Service may not be bound to expected IP: $bind_ip"
        log_warn "Listening on: $listening"
    fi
    
    if command -v curl >/dev/null 2>&1; then
        if curl -s --connect-timeout 5 --max-time 10 "http://${bind_ip}:9100/metrics" 2>/dev/null | grep -q "node_exporter_build_info"; then
            log_info "Metrics endpoint is accessible"
        else
            log_warn "Metrics endpoint test failed"
        fi
    fi
    
    log_info "Installation verified successfully"
    return 0
}

print_summary() {
    local bind_ip="$1"
    local version="$2"
    
    echo ""
    log_info "=========================================="
    log_info "Installation Summary"
    log_info "=========================================="
    log_info "Version: $version"
    log_info "Binary: /usr/local/bin/node_exporter"
    log_info "Service: node_exporter.service"
    log_info "Bind Address: ${bind_ip}:9100"
    log_info "Metrics URL: http://${bind_ip}:9100/metrics"
    echo ""
    log_info "Useful commands:"
    log_info "  Status:    sudo systemctl status node_exporter"
    log_info "  Logs:      sudo journalctl -u node_exporter -f"
    log_info "  Restart:   sudo systemctl restart node_exporter"
    log_info "  Stop:      sudo systemctl stop node_exporter"
    echo ""
    log_info "Next steps:"
    log_info "1. On the central server, add this host as a Prometheus target:"
    log_info "   prometheus-target add ${bind_ip}:9100 host=$(hostname -f) role=ROLE network=NETWORK"
    log_info "2. Prometheus will pick up the new target within 5 minutes (no restart needed)"
    log_info "3. Verify in Prometheus UI: Status > Targets"
    echo ""
}

main() {
    local version
    local arch
    local bind_ip
    local central_ip

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --overwrite)
                OVERWRITE_FLAG=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    log_info "=========================================="
    log_info "Prometheus Node Exporter Installer"
    log_info "=========================================="
    echo ""

    need_root
    log_info "Root check passed"
    
    check_required_commands
    log_info "Required commands check passed"
    
    if check_existing_installation; then
        log_info "Existing installation detected, will upgrade"
    else
        log_info "No existing installation found"
    fi
    
    log_info "Checking for latest version..."
    version=$(get_latest_version)
    log_info "Version check completed: $version"
    
    arch=$(detect_architecture)
    log_info "Architecture detected: $arch"
    echo ""
    
    bind_ip=$(prompt_ip_address)
    echo ""
    log_info "IP address selected: $bind_ip"
    
    if ! check_port_availability "$bind_ip"; then
        log_error "Cannot proceed: port 9100 is already in use"
        exit 1
    fi
    
    central_ip=$(prompt_central_server_ip)
    
    echo ""
    log_info "Installation configuration:"
    log_info "  Version: $version"
    log_info "  Architecture: $arch"
    log_info "  Bind IP: $bind_ip"
    [ -n "$central_ip" ] && log_info "  Central Server IP: $central_ip" || log_info "  Central Server IP: (not configured)"
    echo ""
    
    log_prompt "Proceed with installation? [y/N]"
    read -r confirm < /dev/tty
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "Installation cancelled"
        exit 0
    fi
    
    echo ""
    install_node_exporter "$version" "$arch" "$bind_ip"
    create_system_user
    create_systemd_service "$bind_ip"
    configure_firewall "$bind_ip" "$central_ip"
    enable_and_start_service
    verify_installation "$bind_ip"
    print_summary "$bind_ip" "$version"
    
    log_info "Installation completed successfully!"
}

main "$@"

