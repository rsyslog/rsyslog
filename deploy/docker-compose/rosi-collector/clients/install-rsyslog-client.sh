#!/usr/bin/env bash
# install-rsyslog-client.sh
# Automated installation script for rsyslog client configuration
# Configures rsyslog to forward logs to ROSI Collector server
#
# Usage:
#   sudo ./install-rsyslog-client.sh
#
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1" >&2; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $1" >&2; }
log_prompt() { echo -e "${BLUE}[PROMPT]${NC} $1" >&2; }

INSTALL_SIDECAR_SET="false"
LISTEN_ADDR_SET="false"
LISTEN_PORT_SET="false"
IMPSTATS_UDP_ADDR_SET="false"
IMPSTATS_UDP_PORT_SET="false"

if [ -n "${INSTALL_SIDECAR+x}" ]; then INSTALL_SIDECAR_SET="true"; fi
if [ -n "${LISTEN_ADDR+x}" ]; then LISTEN_ADDR_SET="true"; fi
if [ -n "${LISTEN_PORT+x}" ]; then LISTEN_PORT_SET="true"; fi
if [ -n "${IMPSTATS_UDP_ADDR+x}" ]; then IMPSTATS_UDP_ADDR_SET="true"; fi
if [ -n "${IMPSTATS_UDP_PORT+x}" ]; then IMPSTATS_UDP_PORT_SET="true"; fi

INSTALL_SIDECAR="${INSTALL_SIDECAR:-true}"
LISTEN_ADDR="${LISTEN_ADDR:-127.0.0.1}"
LISTEN_PORT="${LISTEN_PORT:-9898}"
IMPSTATS_UDP_ADDR="${IMPSTATS_UDP_ADDR:-127.0.0.1}"
IMPSTATS_UDP_PORT="${IMPSTATS_UDP_PORT:-19090}"
SIDECAR_PREFIX="${SIDECAR_PREFIX:-/opt/rsyslog-exporter}"

to_lower() {
    echo "$1" | tr 'A-Z' 'a-z'
}

is_true() {
    case "$(to_lower "$1")" in
        1|true|yes|y|on) return 0 ;;
        *) return 1 ;;
    esac
}

is_false() {
    case "$(to_lower "$1")" in
        0|false|no|n|off) return 0 ;;
        *) return 1 ;;
    esac
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
    
    for cmd in systemctl rsyslogd; do
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
        log_error "Either curl or wget is required to download configuration file"
        exit 1
    fi
    
    if ! systemctl is-enabled rsyslog.service >/dev/null 2>&1; then
        if ! systemctl list-unit-files --type=service 2>/dev/null | grep -q "^rsyslog.service"; then
            log_error "rsyslog service not found"
            log_error "Please install rsyslog: sudo apt-get install rsyslog"
            exit 1
        fi
    fi
}

check_sidecar_requirements() {
    local missing=()
    
    for cmd in python3 tar; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "Missing required commands for sidecar install: ${missing[*]}"
        log_error "Please install the required packages and try again"
        exit 1
    fi

    if ! python3 -m venv -h >/dev/null 2>&1 || ! python3 -m ensurepip --version >/dev/null 2>&1; then
        if [ -f /etc/os-release ]; then
            . /etc/os-release
        fi
        if command -v apt-get >/dev/null 2>&1 && [[ "${ID:-}" =~ ^(ubuntu|debian)$ ]]; then
            local py_ver
            local venv_pkg
            py_ver=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null || echo "")
            if [ -n "$py_ver" ]; then
                venv_pkg="python${py_ver}-venv"
            else
                venv_pkg="python3-venv"
            fi
            if [ -t 0 ]; then
                log_warn "python3 venv/ensurepip module is missing."
                log_prompt "Run 'apt update && apt install ${venv_pkg}'? [Y/n]"
                read -r confirm < /dev/tty
                if [[ "$confirm" =~ ^[Nn]$ ]]; then
                    log_error "python3 venv module is required for sidecar install"
                    exit 1
                fi
                apt update
                if ! apt install -y "${venv_pkg}"; then
                    if [ "${venv_pkg}" != "python3-venv" ]; then
                        log_warn "Failed to install ${venv_pkg}, trying python3-venv..."
                        apt install -y python3-venv
                    fi
                fi
            else
                log_error "python3 venv module is missing. Install python3-venv and retry."
                exit 1
            fi
        else
            log_error "python3 venv module is missing. Install python3-venv and retry."
            exit 1
        fi
    fi
}

configure_impstats_firewall() {
    local central_ip="$1"
    local central_ips_v4=()
    local central_ips_v6=()
    local configured=false

    if [ "$LISTEN_ADDR" = "127.0.0.1" ]; then
        log_info "Impstats exporter is bound to 127.0.0.1; skipping firewall configuration"
        return 0
    fi

    if [ -f /etc/os-release ]; then
        . /etc/os-release
    fi

    if [[ "${ID:-}" =~ ^(ubuntu|debian)$ ]]; then
        if validate_ip "$central_ip"; then
            central_ips_v4=("$central_ip")
        elif command -v getent >/dev/null 2>&1; then
            while IFS= read -r ip; do
                [ -n "$ip" ] && central_ips_v4+=("$ip")
            done < <(getent ahostsv4 "$central_ip" 2>/dev/null | awk '{print $1}' | sort -u)
            while IFS= read -r ip; do
                [ -n "$ip" ] && central_ips_v6+=("$ip")
            done < <(getent ahostsv6 "$central_ip" 2>/dev/null | awk '{print $1}' | sort -u)
        fi

        if [ ${#central_ips_v4[@]} -eq 0 ] && [ ${#central_ips_v6[@]} -eq 0 ]; then
            log_warn "Central server name did not resolve; skipping firewall rule for impstats exporter"
        fi

        if [ -f "/etc/iptables/rules.v4" ] && command -v iptables >/dev/null 2>&1 && [ ${#central_ips_v4[@]} -gt 0 ]; then
            log_info "Configuring iptables-persistent rule for impstats exporter..."
            local backup_file="/etc/iptables/rules.v4.backup.$(date +%Y%m%d_%H%M%S)"
            cp /etc/iptables/rules.v4 "$backup_file" 2>/dev/null || true
            for src_ip in "${central_ips_v4[@]}"; do
                local rule_desc="$src_ip -> $LISTEN_ADDR:$LISTEN_PORT"
                if iptables -C INPUT -d "$LISTEN_ADDR" -s "$src_ip" -p tcp --dport "$LISTEN_PORT" -j ACCEPT 2>/dev/null; then
                    log_info "iptables rule for $rule_desc already exists"
                    continue
                fi
                if iptables -I INPUT 1 -d "$LISTEN_ADDR" -s "$src_ip" -p tcp --dport "$LISTEN_PORT" -j ACCEPT 2>/dev/null; then
                    configured=true
                else
                    log_warn "Failed to add iptables rule for $rule_desc"
                fi
            done
            if [ "$configured" = true ] && command -v iptables-save >/dev/null 2>&1; then
                iptables-save > /etc/iptables/rules.v4 2>/dev/null || {
                    log_warn "Failed to save iptables rules to /etc/iptables/rules.v4"
                    log_warn "Backup available at: $backup_file"
                }
            fi
        fi

        if [[ "$LISTEN_ADDR" == *:* ]] && [ -f "/etc/iptables/rules.v6" ] && command -v ip6tables >/dev/null 2>&1 && [ ${#central_ips_v6[@]} -gt 0 ]; then
            log_info "Configuring iptables-persistent IPv6 rule for impstats exporter..."
            local backup_file_v6="/etc/iptables/rules.v6.backup.$(date +%Y%m%d_%H%M%S)"
            cp /etc/iptables/rules.v6 "$backup_file_v6" 2>/dev/null || true
            for src_ip in "${central_ips_v6[@]}"; do
                local rule_desc="$src_ip -> $LISTEN_ADDR:$LISTEN_PORT"
                if ip6tables -C INPUT -d "$LISTEN_ADDR" -s "$src_ip" -p tcp --dport "$LISTEN_PORT" -j ACCEPT 2>/dev/null; then
                    log_info "ip6tables rule for $rule_desc already exists"
                    continue
                fi
                if ip6tables -I INPUT 1 -d "$LISTEN_ADDR" -s "$src_ip" -p tcp --dport "$LISTEN_PORT" -j ACCEPT 2>/dev/null; then
                    configured=true
                else
                    log_warn "Failed to add ip6tables rule for $rule_desc"
                fi
            done
            if [ "$configured" = true ] && command -v ip6tables-save >/dev/null 2>&1; then
                ip6tables-save > /etc/iptables/rules.v6 2>/dev/null || {
                    log_warn "Failed to save ip6tables rules to /etc/iptables/rules.v6"
                    log_warn "Backup available at: $backup_file_v6"
                }
            fi
        fi

        if command -v ufw >/dev/null 2>&1 && ufw status 2>/dev/null | grep -q "Status: active"; then
            log_info "Configuring UFW rule for impstats exporter on ${LISTEN_ADDR}:${LISTEN_PORT}..."
            if [ ${#central_ips_v4[@]} -gt 0 ] || [ ${#central_ips_v6[@]} -gt 0 ]; then
                for src_ip in "${central_ips_v4[@]}"; do
                    if ufw allow from "$src_ip" to "$LISTEN_ADDR" port "$LISTEN_PORT" proto tcp comment "rsyslog impstats exporter" 2>/dev/null; then
                        configured=true
                    else
                        log_warn "Failed to add UFW rule (may already exist)"
                    fi
                done
                for src_ip in "${central_ips_v6[@]}"; do
                    if ufw allow from "$src_ip" to "$LISTEN_ADDR" port "$LISTEN_PORT" proto tcp comment "rsyslog impstats exporter" 2>/dev/null; then
                        configured=true
                    else
                        log_warn "Failed to add UFW rule (may already exist)"
                    fi
                done
            else
                log_warn "Central server name did not resolve; skipping UFW rule"
            fi
        fi
    fi

    if [ "$configured" = false ]; then
        log_warn "No firewall rule configured for impstats exporter"
    fi
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

validate_port() {
    local port="$1"
    if [[ $port =~ ^[0-9]+$ ]] && [ "$port" -ge 1 ] && [ "$port" -le 65535 ]; then
        return 0
    fi
    return 1
}

validate_sidecar_params() {
    if ! validate_ip "$LISTEN_ADDR"; then
        log_error "Invalid sidecar listen address: $LISTEN_ADDR"
        return 1
    fi
    if ! validate_ip "$IMPSTATS_UDP_ADDR"; then
        log_error "Invalid impstats UDP address: $IMPSTATS_UDP_ADDR"
        return 1
    fi
    if ! validate_port "$LISTEN_PORT"; then
        log_error "Invalid sidecar listen port: $LISTEN_PORT"
        return 1
    fi
    if ! validate_port "$IMPSTATS_UDP_PORT"; then
        log_error "Invalid impstats UDP port: $IMPSTATS_UDP_PORT"
        return 1
    fi
    return 0
}

validate_domain() {
    local domain="$1"
    if [[ $domain =~ ^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$ ]]; then
        return 0
    fi
    return 1
}

validate_ip_or_domain() {
    local address="$1"
    if validate_ip "$address" || validate_domain "$address"; then
        return 0
    fi
    return 1
}

detect_local_ips() {
    local ips=()
    local ip
    
    while IFS= read -r ip; do
        [ -n "$ip" ] && validate_ip "$ip" && ips+=("$ip")
    done < <(ip addr show 2>/dev/null | grep -E "inet (10\.|192\.168\.|172\.(1[6-9]|2[0-9]|3[01])\.)" | awk '{print $2}' | cut -d/ -f1 || true)
    
    printf '%s\n' "${ips[@]}"
}

detect_all_ips() {
    local ips=()
    local ip
    
    while IFS= read -r ip; do
        [ -n "$ip" ] && validate_ip "$ip" && ips+=("$ip")
    done < <(ip addr show 2>/dev/null | grep "inet " | awk '{print $2}' | cut -d/ -f1 || true)
    
    printf '%s\n' "${ips[@]}"
}

select_listen_ip() {
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
        log_prompt "Select IP address to bind exporter [1-${#all_ips[@]}] or enter custom IP:"
        read -r user_input < /dev/tty
        if [[ "$user_input" =~ ^[0-9]+$ ]] && [ "$user_input" -ge 1 ] && [ "$user_input" -le ${#all_ips[@]} ]; then
            echo "${all_ips[$((user_input-1))]}"
            return
        fi
        while ! validate_ip "$user_input"; do
            log_error "Invalid IP address format: $user_input"
            log_prompt "Please enter a valid IP address (e.g., 10.135.0.10 or 127.0.0.1):"
            read -r user_input < /dev/tty
            if [[ "$user_input" =~ ^[0-9]+$ ]] && [ "$user_input" -ge 1 ] && [ "$user_input" -le ${#all_ips[@]} ]; then
                echo "${all_ips[$((user_input-1))]}"
                return
            fi
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
    
    log_prompt "Select IP address to bind exporter [1-${#all_ips[@]}] or enter custom IP:"
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

show_usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --no-sidecar                 Skip impstats sidecar install (default: enabled)
  --sidecar                    Force impstats sidecar install
  --sidecar-prefix <path>      Sidecar install prefix (default: /opt/rsyslog-exporter)
  --sidecar-listen-addr <ip>   Sidecar HTTP listen address (default: 127.0.0.1)
  --sidecar-listen-port <port> Sidecar HTTP listen port (default: 9898)
  --impstats-udp-addr <ip>     UDP bind addr for impstats (default: 127.0.0.1)
  --impstats-udp-port <port>   UDP port for impstats (default: 19090)
  -h, --help                   Show this help
EOF
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --no-sidecar)
                INSTALL_SIDECAR="false"
                INSTALL_SIDECAR_SET="true"
                shift
                ;;
            --sidecar)
                INSTALL_SIDECAR="true"
                INSTALL_SIDECAR_SET="true"
                shift
                ;;
            --sidecar-prefix)
                [ $# -ge 2 ] || { log_error "--sidecar-prefix requires an argument"; exit 2; }
                SIDECAR_PREFIX="$2"
                shift 2
                ;;
            --sidecar-listen-addr)
                [ $# -ge 2 ] || { log_error "--sidecar-listen-addr requires an argument"; exit 2; }
                LISTEN_ADDR="$2"
                LISTEN_ADDR_SET="true"
                shift 2
                ;;
            --sidecar-listen-port)
                [ $# -ge 2 ] || { log_error "--sidecar-listen-port requires an argument"; exit 2; }
                LISTEN_PORT="$2"
                LISTEN_PORT_SET="true"
                shift 2
                ;;
            --impstats-udp-addr)
                [ $# -ge 2 ] || { log_error "--impstats-udp-addr requires an argument"; exit 2; }
                IMPSTATS_UDP_ADDR="$2"
                IMPSTATS_UDP_ADDR_SET="true"
                shift 2
                ;;
            --impstats-udp-port)
                [ $# -ge 2 ] || { log_error "--impstats-udp-port requires an argument"; exit 2; }
                IMPSTATS_UDP_PORT="$2"
                IMPSTATS_UDP_PORT_SET="true"
                shift 2
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                log_error "Unknown argument: $1"
                show_usage >&2
                exit 2
                ;;
        esac
    done
}

prompt_target_ip() {
    local user_input
    
    log_prompt "Enter central server IP address or domain name:"
    log_info "Press Enter to use default: __TRAEFIK_DOMAIN__"
    read -r user_input < /dev/tty
    if [ -z "$user_input" ]; then
        log_info "Using default: __TRAEFIK_DOMAIN__"
        echo "__TRAEFIK_DOMAIN__"
        return
    fi
    while ! validate_ip_or_domain "$user_input"; do
        log_error "Invalid IP address or domain format: $user_input"
        log_prompt "Please enter a valid IP address (e.g., 10.135.0.10) or domain name (e.g., logs.example.com):"
        read -r user_input < /dev/tty
        if [ -z "$user_input" ]; then
            log_info "Using default: __TRAEFIK_DOMAIN__"
            echo "__TRAEFIK_DOMAIN__"
            return
        fi
    done
    
    echo "$user_input"
}

prompt_target_port() {
    local user_input
    local default_port="10514"
    
    log_prompt "Enter central server port [default: $default_port]:"
    read -r user_input < /dev/tty
    if [ -z "$user_input" ]; then
        echo "$default_port"
        return
    fi
    
    while ! validate_port "$user_input"; do
        log_error "Invalid port number: $user_input"
        log_prompt "Please enter a valid port (1-65535) or press Enter for default ($default_port):"
        read -r user_input < /dev/tty
        if [ -z "$user_input" ]; then
            echo "$default_port"
            return
        fi
    done
    
    echo "$user_input"
}

prompt_sidecar_install() {
    if [ "$INSTALL_SIDECAR_SET" = "true" ]; then
        return 0
    fi
    if [ ! -t 0 ]; then
        return 0
    fi
    log_prompt "Install rsyslog impstats sidecar? [Y/n]"
    read -r confirm < /dev/tty
    if [[ "$confirm" =~ ^[Nn]$ ]]; then
        INSTALL_SIDECAR="false"
    else
        INSTALL_SIDECAR="true"
    fi
}

prompt_sidecar_listen_addr() {
    if [ "$LISTEN_ADDR_SET" = "true" ] || [ ! -t 0 ]; then
        return 0
    fi
    LISTEN_ADDR="$(select_listen_ip)"
    LISTEN_ADDR_SET="true"
}

prompt_sidecar_listen_port() {
    local user_input
    if [ "$LISTEN_PORT_SET" = "true" ] || [ ! -t 0 ]; then
        return 0
    fi
    log_prompt "Enter sidecar listen port [default: $LISTEN_PORT]:"
    read -r user_input < /dev/tty
    if [ -z "$user_input" ]; then
        return 0
    fi
    while ! validate_port "$user_input"; do
        log_error "Invalid port number: $user_input"
        log_prompt "Please enter a valid port (1-65535) or press Enter for default ($LISTEN_PORT):"
        read -r user_input < /dev/tty
        [ -z "$user_input" ] && return 0
    done
    LISTEN_PORT="$user_input"
    LISTEN_PORT_SET="true"
}

prompt_impstats_udp_addr() {
    local user_input
    if [ "$IMPSTATS_UDP_ADDR_SET" = "true" ] || [ ! -t 0 ]; then
        return 0
    fi
    log_prompt "Enter impstats UDP target address [default: $IMPSTATS_UDP_ADDR]:"
    read -r user_input < /dev/tty
    if [ -z "$user_input" ]; then
        return 0
    fi
    while ! validate_ip "$user_input"; do
        log_error "Invalid IP address: $user_input"
        log_prompt "Please enter a valid IP address (e.g., 127.0.0.1):"
        read -r user_input < /dev/tty
        [ -z "$user_input" ] && return 0
    done
    IMPSTATS_UDP_ADDR="$user_input"
    IMPSTATS_UDP_ADDR_SET="true"
}

prompt_impstats_udp_port() {
    local user_input
    if [ "$IMPSTATS_UDP_PORT_SET" = "true" ] || [ ! -t 0 ]; then
        return 0
    fi
    log_prompt "Enter impstats UDP port [default: $IMPSTATS_UDP_PORT]:"
    read -r user_input < /dev/tty
    if [ -z "$user_input" ]; then
        return 0
    fi
    while ! validate_port "$user_input"; do
        log_error "Invalid port number: $user_input"
        log_prompt "Please enter a valid port (1-65535) or press Enter for default ($IMPSTATS_UDP_PORT):"
        read -r user_input < /dev/tty
        [ -z "$user_input" ] && return 0
    done
    IMPSTATS_UDP_PORT="$user_input"
    IMPSTATS_UDP_PORT_SET="true"
}

check_existing_config() {
    local config_file="/etc/rsyslog.d/99-forward-to-central.conf"
    
    if [ -f "$config_file" ]; then
        log_warn "Existing rsyslog client configuration found: $config_file"
        log_info "Current configuration:"
        cat "$config_file" | sed 's/^/  /' >&2
        echo "" >&2
        log_prompt "Overwrite existing configuration? [y/N]"
        read -r confirm < /dev/tty
        if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
            log_info "Installation cancelled"
            exit 0
        fi
        return 0
    fi
    return 1
}

get_script_dir() {
    local script_path="${BASH_SOURCE[0]}"
    if [ -L "$script_path" ]; then
        script_path=$(readlink -f "$script_path")
    fi
    dirname "$script_path"
}

CENTRAL_SERVER_DOMAIN="${CENTRAL_SERVER_DOMAIN:-__TRAEFIK_DOMAIN__}"

detect_central_server_domain() {
    local script_path="${BASH_SOURCE[0]}"
    local script_dir
    local domain
    local preconfigured_domain
    
    script_dir=$(get_script_dir)
    
    preconfigured_domain="${CENTRAL_SERVER_DOMAIN}"
    
    if [ -n "$preconfigured_domain" ] && [ "$preconfigured_domain" != "__TRAEFIK_DOMAIN__" ]; then
        log_info "Using pre-configured central server domain: $preconfigured_domain"
        echo "$preconfigured_domain"
        return
    fi
    
    log_info "Detecting central server domain..."
    
    if [[ "$script_dir" =~ ^/tmp/ ]] || [[ "$script_dir" =~ ^/var/tmp/ ]] || [[ "$script_dir" =~ ^/root/ ]] || [[ "$script_dir" =~ ^/home/ ]]; then
        log_info "Script appears to be downloaded (not in repository)"
    fi
    
    log_prompt "Enter central server domain or IP address for downloading config (e.g., logs.example.com or 10.135.0.10):"
    log_info "This will be used to download: https://__TRAEFIK_DOMAIN__/downloads/rsyslog-forward-minimal.conf"
    log_info "Press Enter to use default: __TRAEFIK_DOMAIN__"
    read -r domain < /dev/tty
    if [ -z "$domain" ]; then
        log_info "Using default domain: __TRAEFIK_DOMAIN__"
        echo "__TRAEFIK_DOMAIN__"
        return
    fi
    echo "$domain"
}

download_config_template() {
    local central_domain="$1"
    local temp_dir
    local config_url
    local temp_file
    local protocol="https"
    
    temp_dir=$(mktemp -d)
    temp_file="${temp_dir}/rsyslog-forward-minimal.conf"
    
    if validate_ip "$central_domain"; then
        protocol="http"
        log_warn "IP address detected, using HTTP instead of HTTPS"
    fi
    
    config_url="${protocol}://${central_domain}/downloads/rsyslog-forward-minimal.conf"
    
    log_info "Downloading configuration template from: $config_url"
    
    if command -v curl >/dev/null 2>&1; then
        if curl -sSL --connect-timeout 10 --max-time 30 -o "$temp_file" "$config_url" 2>&1; then
            if [ -f "$temp_file" ] && [ -s "$temp_file" ]; then
                if grep -q "CHANGE_ME" "$temp_file" 2>/dev/null; then
                    echo "$temp_file"
                    return 0
                else
                    log_warn "Downloaded file doesn't appear to be a valid template (missing CHANGE_ME)"
                fi
            fi
        fi
    elif command -v wget >/dev/null 2>&1; then
        if wget -q --timeout=30 -O "$temp_file" "$config_url" 2>&1; then
            if [ -f "$temp_file" ] && [ -s "$temp_file" ]; then
                if grep -q "CHANGE_ME" "$temp_file" 2>/dev/null; then
                    echo "$temp_file"
                    return 0
                else
                    log_warn "Downloaded file doesn't appear to be a valid template (missing CHANGE_ME)"
                fi
            fi
        fi
    fi
    
    log_error "Failed to download configuration template from $config_url"
    log_error "Please verify the central server domain is correct and accessible"
    log_error "You can also manually download the file and place it in the same directory as this script"
    rm -rf "$temp_dir" 2>/dev/null || true
    return 1
}

download_sidecar_bundle() {
    local central_domain="$1"
    local temp_dir
    local bundle_url
    local bundle_file
    local protocol="https"
    
    temp_dir=$(mktemp -d)
    bundle_file="${temp_dir}/rsyslog-exporter.tar.gz"
    
    if validate_ip "$central_domain"; then
        protocol="http"
        log_warn "IP address detected, using HTTP instead of HTTPS for sidecar download"
    fi
    
    bundle_url="${protocol}://${central_domain}/downloads/rsyslog-exporter.tar.gz"
    log_info "Downloading rsyslog exporter bundle from: $bundle_url"
    
    if command -v curl >/dev/null 2>&1; then
        if curl -sSL --connect-timeout 10 --max-time 60 -o "$bundle_file" "$bundle_url" 2>&1; then
            if [ -s "$bundle_file" ]; then
                echo "$bundle_file"
                return 0
            fi
        fi
    elif command -v wget >/dev/null 2>&1; then
        if wget -q --timeout=60 -O "$bundle_file" "$bundle_url" 2>&1; then
            if [ -s "$bundle_file" ]; then
                echo "$bundle_file"
                return 0
            fi
        fi
    fi
    
    log_error "Failed to download rsyslog exporter bundle from $bundle_url"
    rm -rf "$temp_dir" 2>/dev/null || true
    return 1
}

install_impstats_config() {
    local config_file="/etc/rsyslog.d/10-impstats.conf"
    local tmpfile
    
    if [ -f "$config_file" ] && [ -t 0 ]; then
        log_warn "Existing impstats config found: $config_file"
        log_prompt "Overwrite existing impstats config? [y/N]"
        read -r confirm < /dev/tty
        if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
            log_info "Keeping existing impstats config"
            return 0
        fi
    fi
    
    tmpfile=$(mktemp)
    cat > "$tmpfile" <<EOF
# rsyslog impstats -> UDP -> rsyslog_exporter sidecar
#
# This configuration emits impstats in JSON format and forwards ONLY the JSON
# payload (no syslog header) via UDP to the local sidecar listener.

# Send only the message payload (JSON) plus newline.
# The sidecar expects one JSON object per line.
template(name="impstatsJsonOnly" type="string" string="%msg%\\n")

ruleset(name="impstats_to_exporter") {
  action(
    type="omfwd"
    target="${IMPSTATS_UDP_ADDR}"
    port="${IMPSTATS_UDP_PORT}"
    protocol="udp"
    template="impstatsJsonOnly"
  )

  # Stop further processing of impstats messages.
  stop
}

# Emit impstats every 30 seconds and route them to the forwarding ruleset.
# Note: when using a ruleset, syslog stream processing must be enabled, so do
# not set logSyslog/log.syslog to "off".
# bracketing="on" groups each interval's burst cleanly.
module(load="impstats"
       interval="30"
       format="json"
       ruleset="impstats_to_exporter"
       bracketing="on"
       resetCounters="off")
EOF
    
    install -m 0644 "$tmpfile" "$config_file"
    rm -f "$tmpfile"
    log_info "Installed impstats config: $config_file"
}

install_sidecar_service() {
    local central_domain="$1"
    local script_dir
    local local_installer
    local bundle_file=""
    local bundle_dir=""
    local installer=""
    local overwrite_flag=""
    
    script_dir=$(get_script_dir)
    local_installer="${script_dir}/../../../../sidecar/install-service.sh"
    
    if [ -x "$local_installer" ]; then
        installer="$local_installer"
    else
        bundle_file=$(download_sidecar_bundle "$central_domain") || return 1
        bundle_dir=$(dirname "$bundle_file")
        tar -xzf "$bundle_file" -C "$bundle_dir" || {
            log_error "Failed to extract rsyslog exporter bundle"
            return 1
        }
        installer="${bundle_dir}/sidecar/install-service.sh"
    fi
    
    if [ ! -x "$installer" ]; then
        log_error "Sidecar installer not found: $installer"
        return 1
    fi
    
    if [ -f /etc/systemd/system/rsyslog-exporter.service ]; then
        if [ -t 0 ]; then
            log_warn "rsyslog exporter service already exists"
            log_prompt "Overwrite existing rsyslog-exporter service? [y/N]"
            read -r confirm < /dev/tty
            if [[ "$confirm" =~ ^[Yy]$ ]]; then
                overwrite_flag="--overwrite"
            else
                log_info "Skipping sidecar install"
                return 0
            fi
        else
            overwrite_flag="--overwrite"
        fi
    fi
    
    log_info "Installing rsyslog exporter sidecar service..."
    "$installer" \
        --prefix "$SIDECAR_PREFIX" \
        --listen-addr "$LISTEN_ADDR" \
        --listen-port "$LISTEN_PORT" \
        --impstats-mode udp \
        --impstats-format json \
        --impstats-udp-addr "$IMPSTATS_UDP_ADDR" \
        --impstats-udp-port "$IMPSTATS_UDP_PORT" \
        ${overwrite_flag} || return 1

    if [ -n "$bundle_dir" ]; then
        rm -rf "$bundle_dir" 2>/dev/null || true
    fi
    
    return 0
}

verify_sidecar_service() {
    if systemctl is-active --quiet rsyslog-exporter 2>/dev/null; then
        log_info "rsyslog exporter sidecar is active"
        return 0
    fi
    log_warn "rsyslog exporter sidecar is not active"
    log_warn "Check logs: journalctl -u rsyslog-exporter -n 50"
    return 1
}

install_rsyslog_config() {
    local target_ip="$1"
    local target_port="$2"
    local central_domain="$3"
    local config_file="/etc/rsyslog.d/99-forward-to-central.conf"
    local template_file
    local temp_dir
    
    log_info "Installing rsyslog client configuration..."
    log_info "  Target: $target_ip:$target_port"
    log_info "  Config file: $config_file"
    
    template_file=$(download_config_template "$central_domain") || {
        log_error "Failed to download configuration template"
        exit 1
    }
    
    temp_dir=$(dirname "$template_file")
    
    trap "rm -rf '$temp_dir'" EXIT
    
    if [ ! -f "$template_file" ]; then
        log_error "Template file not found: $template_file"
        exit 1
    fi
    
    sed "s/CHANGE_ME/$target_ip/g; s/port=\"10514\"/port=\"$target_port\"/g" \
        "$template_file" > "$config_file" || {
        log_error "Failed to create configuration file"
        exit 1
    }
    
    chmod 644 "$config_file" || {
        log_error "Failed to set permissions on configuration file"
        exit 1
    }
    
    rm -rf "$temp_dir"
    trap - EXIT
    
    log_info "Configuration file created successfully"
}

create_spool_directory() {
    local spool_dir="/var/spool/rsyslog"
    
    if [ ! -d "$spool_dir" ]; then
        log_info "Creating spool directory: $spool_dir"
        mkdir -p "$spool_dir" || {
            log_error "Failed to create spool directory"
            exit 1
        }
        chmod 755 "$spool_dir" || {
            log_warn "Failed to set permissions on spool directory"
        }
        log_info "Spool directory created successfully"
    else
        log_info "Spool directory already exists: $spool_dir"
    fi
}

test_rsyslog_config() {
    log_info "Testing rsyslog configuration..."
    
    if rsyslogd -N1 -f /etc/rsyslog.conf 2>&1 | grep -q "error"; then
        log_error "rsyslog configuration test failed"
        log_error "Configuration errors:"
        rsyslogd -N1 -f /etc/rsyslog.conf 2>&1 | grep -i error | sed 's/^/  /' >&2
        return 1
    fi
    
    log_info "Configuration test passed"
    return 0
}

restart_rsyslog() {
    log_info "Restarting rsyslog service..."
    
    systemctl restart rsyslog || {
        log_error "Failed to restart rsyslog service"
        return 1
    }
    
    sleep 2
    
    if systemctl is-active --quiet rsyslog; then
        log_info "rsyslog service restarted successfully"
        return 0
    else
        log_error "rsyslog service failed to start"
        return 1
    fi
}

show_rsyslog_logs() {
    local lines="${1:-50}"
    
    log_info "=========================================="
    log_info "Recent rsyslog logs (last $lines lines)"
    log_info "=========================================="
    echo ""
    
    if command -v journalctl >/dev/null 2>&1; then
        journalctl -u rsyslog -n "$lines" --no-pager || {
            log_warn "Failed to retrieve journal logs, trying syslog..."
            tail -n "$lines" /var/log/syslog 2>/dev/null || {
                log_warn "Failed to retrieve syslog"
            }
        }
    else
        tail -n "$lines" /var/log/syslog 2>/dev/null || {
            log_warn "Failed to retrieve syslog"
        }
    fi
    
    echo ""
    log_info "=========================================="
    log_info "Checking for configuration errors"
    log_info "=========================================="
    echo ""
    
    if journalctl -u rsyslog --since "1 minute ago" 2>/dev/null | grep -i "error\|warning\|fail" | head -20; then
        log_warn "Found errors or warnings in recent logs"
    else
        log_info "No errors found in recent logs"
    fi
    
    echo ""
}

verify_installation() {
    local target_ip="$1"
    local target_port="$2"
    local config_file="/etc/rsyslog.d/99-forward-to-central.conf"
    
    log_info "Verifying installation..."
    
    if [ ! -f "$config_file" ]; then
        log_error "Configuration file not found: $config_file"
        return 1
    fi
    
    if ! grep -q "target=\"$target_ip\"" "$config_file"; then
        log_error "Target IP/domain not found in configuration file"
        return 1
    fi
    
    if ! grep -q "port=\"$target_port\"" "$config_file"; then
        log_error "Target port not found in configuration file"
        return 1
    fi
    
    if ! systemctl is-active --quiet rsyslog; then
        log_error "rsyslog service is not active"
        return 1
    fi
    
    log_info "Installation verified successfully"
    return 0
}

print_summary() {
    local target_ip="$1"
    local target_port="$2"
    
    echo ""
    log_info "=========================================="
    log_info "Installation Summary"
    log_info "=========================================="
    log_info "Config file: /etc/rsyslog.d/99-forward-to-central.conf"
    log_info "Target server: $target_ip:$target_port"
    log_info "Spool directory: /var/spool/rsyslog"
    if is_true "$INSTALL_SIDECAR"; then
        log_info "Impstats UDP target: ${IMPSTATS_UDP_ADDR}:${IMPSTATS_UDP_PORT}"
        log_info "Impstats exporter: http://${LISTEN_ADDR}:${LISTEN_PORT}/metrics"
        if [ "$LISTEN_ADDR" = "127.0.0.1" ]; then
            log_warn "Exporter is bound to 127.0.0.1 (local-only). Prometheus on the collector will not reach it."
        fi
        log_info "Node exporter port: 9100"
    fi
    echo ""
    log_info "Useful commands:"
    log_info "  Status:    sudo systemctl status rsyslog"
    log_info "  Logs:      sudo journalctl -u rsyslog -f"
    log_info "  Restart:   sudo systemctl restart rsyslog"
    log_info "  Test:      logger 'test message'"
    log_info "  Config:    sudo rsyslogd -N1 -f /etc/rsyslog.conf"
    echo ""
    log_info "Next steps:"
    log_info "1. Verify logs are being forwarded:"
    log_info "   logger 'test message from $(hostname)'"
    log_info "2. Check central server logs to confirm receipt"
    if is_true "$INSTALL_SIDECAR"; then
        log_info "3. Add this host to Prometheus impstats targets:"
        log_info "   prometheus-target --job impstats add ${LISTEN_ADDR}:${LISTEN_PORT} host=$(hostname -f) role=rsyslog"
    fi
    echo ""
}

main() {
    local target_ip
    local target_port
    local central_domain
    
    parse_args "$@"
    if ! is_true "$INSTALL_SIDECAR" && ! is_false "$INSTALL_SIDECAR"; then
        log_error "INSTALL_SIDECAR must be true or false"
        exit 1
    fi
    
    log_info "=========================================="
    log_info "rsyslog Client Configuration Installer"
    log_info "=========================================="
    echo ""
    
    need_root
    log_info "Root check passed"
    
    check_required_commands
    log_info "Required commands check passed"
    echo ""
    
    central_domain=$(detect_central_server_domain)
    echo ""
    
    if check_existing_config; then
        log_info "Will overwrite existing configuration"
    else
        log_info "No existing configuration found"
    fi
    echo ""
    
    target_ip=$(prompt_target_ip)
    echo ""
    
    target_port=$(prompt_target_port)
    echo ""
    
    prompt_sidecar_install
    if is_true "$INSTALL_SIDECAR"; then
        check_sidecar_requirements
        prompt_sidecar_listen_addr
        prompt_sidecar_listen_port
        prompt_impstats_udp_addr
        prompt_impstats_udp_port
        validate_sidecar_params || exit 1
    fi
    
    log_info "Installation configuration:"
    log_info "  Central Server Domain: $central_domain"
    log_info "  Target IP/Domain: $target_ip"
    log_info "  Target Port: $target_port"
    if is_true "$INSTALL_SIDECAR"; then
        log_info "  Sidecar: enabled"
        log_info "  Sidecar listen: ${LISTEN_ADDR}:${LISTEN_PORT}"
        log_info "  Impstats UDP: ${IMPSTATS_UDP_ADDR}:${IMPSTATS_UDP_PORT}"
    else
        log_info "  Sidecar: disabled"
    fi
    echo ""
    
    log_prompt "Proceed with installation? [y/N]"
    read -r confirm < /dev/tty
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        log_info "Installation cancelled"
        exit 0
    fi
    
    echo ""
    install_rsyslog_config "$target_ip" "$target_port" "$central_domain"
    create_spool_directory
    
    if is_true "$INSTALL_SIDECAR"; then
        if ! install_sidecar_service "$central_domain"; then
            log_error "Sidecar installation failed"
            exit 1
        fi
        if [ ! -f /etc/rsyslog.d/10-impstats.conf ]; then
            install_impstats_config
        fi
        configure_impstats_firewall "$target_ip"
    fi
    
    if ! test_rsyslog_config; then
        log_error "Configuration test failed, aborting installation"
        log_error "Please fix configuration errors before proceeding"
        exit 1
    fi
    
    if ! restart_rsyslog; then
        log_error "Failed to restart rsyslog, showing logs..."
        show_rsyslog_logs 100
        exit 1
    fi
    
    show_rsyslog_logs 50
    
    if ! verify_installation "$target_ip" "$target_port"; then
        log_error "Installation verification failed"
        exit 1
    fi
    
    if is_true "$INSTALL_SIDECAR"; then
        verify_sidecar_service || true
    fi
    
    print_summary "$target_ip" "$target_port"
    
    log_info "Installation completed successfully!"
}

main "$@"
