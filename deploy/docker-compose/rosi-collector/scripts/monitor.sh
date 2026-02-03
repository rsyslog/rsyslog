#!/usr/bin/env bash
# monitor.sh
# Comprehensive monitoring and debugging script for ROSI Collector Docker stack (Loki/Grafana/Prometheus)
# Part of Rsyslog Operations Stack Initiative (ROSI)
#
# Usage examples:
#   ./monitor.sh status          # Show container status
#   ./monitor.sh logs            # Show recent logs
#   ./monitor.sh health          # Check health of all services
#   ./monitor.sh debug           # Interactive debugging menu
#   ./monitor.sh shell loki      # Shell into Loki container
#   ./monitor.sh shell grafana   # Shell into Grafana container

set -euo pipefail

# Configuration file locations (same as init.sh)
SYSTEM_CONFIG="/etc/rsyslog/rosi-collector.conf"
USER_CONFIG="${HOME}/.config/rsyslog/rosi-collector.conf"

# Load configuration from file if exists
load_config() {
    local config_file="$1"
    if [ -f "$config_file" ]; then
        while IFS='=' read -r key value; do
            [[ "$key" =~ ^#.*$ ]] && continue
            [[ -z "$key" ]] && continue
            value="${value%\"}"
            value="${value#\"}"
            value="${value%\'}"
            value="${value#\'}"
            case "$key" in
                INSTALL_DIR)
                    export "$key=$value"
                    ;;
            esac
        done < "$config_file"
        return 0
    fi
    return 1
}

# Load config if INSTALL_DIR not set via environment
if [ -z "${INSTALL_DIR:-}" ]; then
    load_config "$USER_CONFIG" 2>/dev/null || load_config "$SYSTEM_CONFIG" 2>/dev/null || true
fi

STACK="rosi-collector"
BASE="${INSTALL_DIR:-/opt/rosi-collector}"
COMPOSE="${BASE}/docker-compose.yml"
ENVFILE="${BASE}/.env"
BACKUP_DIR="${BASE}/backups"

CONTAINERS=("loki" "grafana" "prometheus" "rsyslog" "traefik")

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_debug() { echo -e "${CYAN}[DEBUG]${NC} $1"; }
log_header() { echo -e "${BLUE}=== $1 ===${NC}"; }

check_privileges() {
    if [ "${EUID:-$(id -u)}" -ne 0 ]; then
        log_error "This script requires root privileges or sudo"
        log_info "Run with: sudo $0 $*"
        exit 1
    fi
}

check_docker() {
    if ! systemctl is-active --quiet docker; then
        log_error "Docker service is not running"
        log_info "Start Docker with: systemctl start docker"
        exit 1
    fi
}

check_compose() {
    if [ ! -f "${COMPOSE}" ]; then
        log_error "Docker compose file not found: ${COMPOSE}"
        log_info "Run scripts/init.sh first to set up the environment"
        exit 1
    fi
}

# Get the docker-compose project name from the directory
get_project_name() {
    # Try to get from docker-compose config, fallback to directory name
    if [ -f "${COMPOSE}" ]; then
        cd "${BASE}"
        local name=$(docker compose ps --format json 2>/dev/null | head -1 | grep -o '"Project":"[^"]*"' | cut -d'"' -f4)
        if [ -n "$name" ]; then
            echo "$name"
            return
        fi
    fi
    # Fallback: use directory name
    basename "${BASE}"
}

# Cache the project name
PROJECT_NAME=""
get_cached_project_name() {
    if [ -z "$PROJECT_NAME" ]; then
        PROJECT_NAME=$(get_project_name)
    fi
    echo "$PROJECT_NAME"
}

get_container_name() {
    local service="$1"
    local project=$(get_cached_project_name)
    
    # Special cases for containers with custom names in docker-compose
    case "$service" in
        "traefik") 
            # Check if custom name exists, otherwise use project-based name
            if docker ps -a --format "{{.Names}}" | grep -q "^traefik-"; then
                docker ps -a --format "{{.Names}}" | grep "^traefik-" | head -1
            else
                echo "${project}-traefik-1"
            fi
            ;;
        "prometheus")
            if docker ps -a --format "{{.Names}}" | grep -q "^prometheus-"; then
                docker ps -a --format "{{.Names}}" | grep "^prometheus-" | head -1
            else
                echo "${project}-prometheus-1"
            fi
            ;;
        "rsyslog")
            if docker ps -a --format "{{.Names}}" | grep -q "^rsyslog-"; then
                docker ps -a --format "{{.Names}}" | grep "^rsyslog-" | head -1
            else
                echo "${project}-rsyslog-1"
            fi
            ;;
        "downloads")
            if docker ps -a --format "{{.Names}}" | grep -q "^downloads-"; then
                docker ps -a --format "{{.Names}}" | grep "^downloads-" | head -1
            else
                echo "${project}-downloads-1"
            fi
            ;;
        *)
            # Default pattern: project-service-1
            echo "${project}-${service}-1"
            ;;
    esac
}

get_container_status() {
    local container="$1"
    if docker ps --format "{{.Names}}" | grep -q "^${container}$"; then
        echo "running"
    elif docker ps -a --format "{{.Names}}" | grep -q "^${container}$"; then
        echo "stopped"
    else
        echo "not_found"
    fi
}

show_status() {
    log_header "Container Status"
    
    cd "${BASE}"
    
    log_info "Docker Compose Status:"
    docker compose ps
    
    echo
    log_info "Individual Container Status:"
    
    for service in "${CONTAINERS[@]}"; do
        local container=$(get_container_name "$service")
        local status=$(get_container_status "$container")
        case "$status" in
            "running")
                log_info "✓ ${service} (${container}): ${GREEN}RUNNING${NC}"
                ;;
            "stopped")
                log_warn "✗ ${service} (${container}): ${YELLOW}STOPPED${NC}"
                ;;
            "not_found")
                log_error "✗ ${service} (${container}): ${RED}NOT FOUND${NC}"
                ;;
        esac
    done
    
    echo
    log_info "Docker Network & Internal IPs:"
    # Find the ROSI collector network by checking which network the containers are actually on
    local project=$(get_cached_project_name)
    local network=""
    local sample_container=$(get_container_name "grafana")
    
    # Get the network name from a running container
    if docker inspect "$sample_container" >/dev/null 2>&1; then
        network=$(docker inspect "$sample_container" --format '{{range $k, $v := .NetworkSettings.Networks}}{{$k}}{{end}}' 2>/dev/null | grep -E "rosi|collector" | head -1)
    fi
    
    # Fallback: try known network names
    if [ -z "$network" ]; then
        for net in "${STACK}-net" "${project}_${STACK}-net" "${project}-${STACK}-net"; do
            if docker network inspect "$net" >/dev/null 2>&1; then
                network="$net"
                break
            fi
        done
    fi
    
    if [ -n "$network" ]; then
        local gateway=$(docker network inspect "$network" --format '{{range .IPAM.Config}}{{.Gateway}}{{end}}' 2>/dev/null)
        local subnet=$(docker network inspect "$network" --format '{{range .IPAM.Config}}{{.Subnet}}{{end}}' 2>/dev/null)
        echo -e "  Network: ${CYAN}${network}${NC}"
        echo -e "  Subnet:  ${subnet}"
        echo -e "  Gateway: ${gateway} (host access from containers)"
    fi
    
    # Always get IPs directly from containers (more reliable than network inspect)
    echo
    printf "  %-25s %-18s\n" "CONTAINER" "INTERNAL IP"
    printf "  %-25s %-18s\n" "---------" "-----------"
    for service in "${CONTAINERS[@]}"; do
        local container=$(get_container_name "$service")
        local ip=$(docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$container" 2>/dev/null)
        if [ -n "$ip" ]; then
            printf "  %-25s %-18s\n" "$container" "$ip"
        fi
    done
    # Also show any rsyslog-tls container
    local tls_container=$(docker ps --format '{{.Names}}' | grep -E "rsyslog.*tls" | head -1)
    if [ -n "$tls_container" ]; then
        local tls_ip=$(docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$tls_container" 2>/dev/null)
        if [ -n "$tls_ip" ]; then
            printf "  %-25s %-18s\n" "$tls_container" "$tls_ip"
        fi
    fi
    # Also show downloads container
    local dl_container=$(docker ps --format '{{.Names}}' | grep -E "downloads" | head -1)
    if [ -n "$dl_container" ]; then
        local dl_ip=$(docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$dl_container" 2>/dev/null)
        if [ -n "$dl_ip" ]; then
            printf "  %-25s %-18s\n" "$dl_container" "$dl_ip"
        fi
    fi
    
    echo
    log_info "Resource Usage:"
    docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}" | head -1
    docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}" | grep -E "(loki|grafana|prometheus|rsyslog|traefik)" || true
}

show_logs() {
    local lines="${1:-50}"
    local follow="${2:-false}"
    
    cd "${BASE}"
    
    if [ "$follow" = "true" ]; then
        log_header "Following Logs (Ctrl+C to stop)"
        log_info "Following all service logs..."
        docker compose logs -f
    else
        log_header "Recent Logs (last ${lines} lines)"
        
        for service in "${CONTAINERS[@]}"; do
            echo
            log_info "${service^} Logs:"
            echo "----------------------------------------"
            docker compose logs --tail="${lines}" "${service}" 2>/dev/null || log_warn "No ${service} logs available"
        done
    fi
}

check_health() {
    log_header "Health Check"
    
    cd "${BASE}"
    
    local all_running=true
    for service in "${CONTAINERS[@]}"; do
        local container=$(get_container_name "$service")
        local status=$(get_container_status "$container")
        if [ "$status" != "running" ]; then
            log_error "✗ ${service} (${container}) is not running"
            all_running=false
        else
            log_info "✓ ${service} (${container}) is running"
        fi
    done
    
    if [ "$all_running" = false ]; then
        return 1
    fi
    
    echo
    log_info "Checking service endpoints..."
    
    if curl -s -f -o /dev/null "http://localhost:3000/api/health" --max-time 5; then
        log_info "✓ Grafana web interface is accessible"
    else
        log_warn "✗ Grafana web interface is not accessible"
    fi
    
    if curl -s -f -o /dev/null "http://localhost:3100/ready" --max-time 5; then
        log_info "✓ Loki is ready"
    else
        log_warn "✗ Loki is not ready"
    fi
    
    # Prometheus is behind Traefik now, check via docker exec
    if docker exec "$(get_container_name prometheus)" wget -q -O /dev/null "http://localhost:9090/-/healthy" 2>/dev/null; then
        log_info "✓ Prometheus is healthy"
    else
        log_warn "✗ Prometheus is not healthy"
    fi
    
    echo
    log_info "Checking SSL certificate expiration..."
    check_ssl_expiration
    
    echo
    log_info "Checking Prometheus targets..."
    check_prometheus_targets
    
    echo
    log_info "Checking log flow (rsyslog -> omhttp -> Loki)..."
    check_log_flow
    
    echo
    log_info "Checking disk space..."
    df -h "${BASE}" | tail -1 | awk '{print "Available space: " $4 " (" $5 " used)"}'
    
    log_info "Checking memory usage..."
    free -h | grep "Mem:" | awk '{print "Memory: " $3 "/" $2 " (" int($3/$2*100) "% used)"}'
}

check_ssl_expiration() {
    local domain=""
    if [ -f "${ENVFILE}" ]; then
        domain=$(grep "^TRAEFIK_DOMAIN=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 | tr -d '"')
    fi
    
    if [ -z "$domain" ]; then
        log_warn "Cannot determine domain from .env file"
        return 1
    fi
    
    local cert_info
    cert_info=$(echo | openssl s_client -servername "$domain" -connect "$domain:443" 2>/dev/null | openssl x509 -noout -dates 2>/dev/null)
    
    if [ -z "$cert_info" ]; then
        log_warn "✗ Could not retrieve SSL certificate for $domain"
        return 1
    fi
    
    local expiry_date
    expiry_date=$(echo "$cert_info" | grep "notAfter" | cut -d'=' -f2)
    
    if [ -z "$expiry_date" ]; then
        log_warn "✗ Could not parse certificate expiration date"
        return 1
    fi
    
    local expiry_epoch
    expiry_epoch=$(date -d "$expiry_date" +%s 2>/dev/null)
    local now_epoch
    now_epoch=$(date +%s)
    local days_left=$(( (expiry_epoch - now_epoch) / 86400 ))
    
    if [ "$days_left" -lt 0 ]; then
        log_error "✗ SSL certificate for $domain has EXPIRED!"
    elif [ "$days_left" -lt 7 ]; then
        log_error "✗ SSL certificate for $domain expires in ${days_left} days (CRITICAL)"
    elif [ "$days_left" -lt 14 ]; then
        log_warn "! SSL certificate for $domain expires in ${days_left} days"
    elif [ "$days_left" -lt 30 ]; then
        log_info "✓ SSL certificate for $domain expires in ${days_left} days"
    else
        log_info "✓ SSL certificate for $domain valid for ${days_left} days (expires: $expiry_date)"
    fi
}

check_prometheus_targets() {
    local targets_json
    # Prometheus is behind Traefik now, access via docker exec
    targets_json=$(docker exec "$(get_container_name prometheus)" wget -q -O - "http://localhost:9090/api/v1/targets" 2>/dev/null) || true
    
    if [ -z "$targets_json" ]; then
        log_warn "✗ Could not retrieve Prometheus targets"
        return 0
    fi
    
    local active_count down_count unknown_count total_count
    # Use tr to strip newlines from wc output; || true prevents set -e from failing on grep no-match
    active_count=$(echo "$targets_json" | grep -o '"health":"up"' | wc -l | tr -d '[:space:]') || true
    down_count=$(echo "$targets_json" | grep -o '"health":"down"' | wc -l | tr -d '[:space:]') || true
    unknown_count=$(echo "$targets_json" | grep -o '"health":"unknown"' | wc -l | tr -d '[:space:]') || true
    # Default to 0 if empty
    active_count=${active_count:-0}
    down_count=${down_count:-0}
    unknown_count=${unknown_count:-0}
    total_count=$((active_count + down_count + unknown_count))
    
    if [ "$down_count" -gt 0 ]; then
        log_warn "! Prometheus targets: ${active_count}/${total_count} up, ${down_count} DOWN"
        # Show which targets are down
        local down_targets
        down_targets=$(echo "$targets_json" | grep -oP '"instance":"[^"]+",".*?"health":"down"' | grep -oP '"instance":"[^"]+"' | cut -d'"' -f4 | head -5) || true
        if [ -n "$down_targets" ]; then
            echo "  Down targets:"
            echo "$down_targets" | while read -r target; do
                echo "    - $target"
            done
        fi
    elif [ "$total_count" -eq 0 ]; then
        log_warn "! No Prometheus targets configured"
    else
        log_info "✓ Prometheus targets: ${active_count}/${total_count} up"
    fi
}

check_log_flow() {
    local test_id="healthcheck-$(date +%s)-$$"
    local test_msg="HEALTH_CHECK_TEST ${test_id}"
    
    # Send test message via logger to rsyslog
    if ! command -v logger >/dev/null 2>&1; then
        log_warn "✗ 'logger' command not available, skipping log flow test"
        return 1
    fi
    
    # Send test syslog message
    logger -n 127.0.0.1 -P 514 -d "${test_msg}" 2>/dev/null || {
        log_warn "✗ Could not send test syslog message"
        return 1
    }
    
    # Wait for message to propagate
    sleep 2
    
    # Check if message arrived in Loki
    local query_result
    query_result=$(curl -s --max-time 10 \
        "http://localhost:3100/loki/api/v1/query_range" \
        --data-urlencode "query={job=\"syslog\"} |= \"${test_id}\"" \
        --data-urlencode "start=$(date -d '1 minute ago' +%s)000000000" \
        --data-urlencode "end=$(date +%s)000000000" \
        --data-urlencode "limit=1" 2>/dev/null)
    
    if echo "$query_result" | grep -q "$test_id"; then
        log_info "✓ Log flow working: rsyslog -> omhttp -> Loki"
    else
        # Check intermediate steps
        echo "  Checking intermediate steps..."
        
        # Check if rsyslog received it (check rsyslog stats)
        local rsyslog_stats
        rsyslog_stats=$(docker exec "$(get_container_name rsyslog)" cat /var/log/pstats.log 2>/dev/null | tail -5)
        if [ -n "$rsyslog_stats" ]; then
            log_info "  - rsyslog is processing messages (pstats available)"
        else
            log_warn "  - rsyslog pstats not available"
        fi
        
        # Check Loki ingestion
        local loki_metrics
        loki_metrics=$(curl -s --max-time 5 "http://localhost:3100/metrics" 2>/dev/null | grep "loki_ingester_chunks_stored_total" | head -1)
        if [ -n "$loki_metrics" ]; then
            log_info "  - Loki is storing chunks"
        else
            log_warn "  - Loki ingestion metrics not found"
        fi
        
        log_warn "! Test message not found in Loki (may need more time or check config)"
    fi
}

debug_menu() {
    while true; do
        echo
        log_header "ROSI Collector Debug Menu"
        echo "1) Show container status"
        echo "2) Show recent logs (50 lines)"
        echo "3) Show recent logs (100 lines)"
        echo "4) Follow logs (real-time)"
        echo "5) Check health"
        echo "6) Shell into Loki container"
        echo "7) Shell into Grafana container"
        echo "8) Shell into Prometheus container"
        echo "9) Shell into rsyslog container"
        echo "10) Shell into Traefik container"
        echo "11) Restart services"
        echo "12) View environment variables"
        echo "13) Check network connectivity"
        echo "14) Show resource usage"
        echo "15) Check Prometheus config"
        echo "16) Test syslog forwarding"
        echo "17) Reset Grafana admin password"
        echo "18) Create backup"
        echo "19) List backups"
        echo "20) Restore backup"
        echo "21) Exit"
        echo
        read -rp "Select option (1-21): " choice
        
        case $choice in
            1) show_status ;;
            2) show_logs 50 ;;
            3) show_logs 100 ;;
            4) 
                log_info "Following logs (Ctrl+C to stop)..."
                cd "${BASE}" && docker compose logs -f
                ;;
            5) check_health ;;
            6) 
                log_info "Opening shell in Loki container..."
                docker exec -it "$(get_container_name loki)" /bin/sh
                ;;
            7) 
                log_info "Opening shell in Grafana container..."
                docker exec -it "$(get_container_name grafana)" /bin/bash
                ;;
            8) 
                log_info "Opening shell in Prometheus container..."
                docker exec -it "$(get_container_name prometheus)" /bin/sh
                ;;
            9) 
                log_info "Opening shell in rsyslog container..."
                docker exec -it "$(get_container_name rsyslog)" /bin/bash
                ;;
            10) 
                log_info "Opening shell in Traefik container..."
                docker exec -it "$(get_container_name traefik)" /bin/sh
                ;;
            11) 
                log_info "Restarting services..."
                cd "${BASE}" && docker compose restart
                ;;
            12) 
                log_info "Environment variables:"
                if [ -f "${ENVFILE}" ]; then
                    grep -v "PASSWORD\|SECRET" "${ENVFILE}" || log_warn "Environment file not found"
                else
                    log_warn "Environment file not found: ${ENVFILE}"
                fi
                ;;
            13) 
                log_info "Network connectivity:"
                docker network ls | grep rosi-collector-net
                docker network inspect rosi-collector-net 2>/dev/null | grep -A 20 "Containers" || log_warn "Network not found"
                ;;
            14) 
                log_info "Resource usage:"
                docker stats --no-stream
                ;;
            15) 
                log_info "Prometheus configuration:"
                docker exec "$(get_container_name prometheus)" cat /etc/prometheus/prometheus.yml 2>/dev/null || log_warn "Cannot read Prometheus config"
                ;;
            16) 
                log_info "Sending test syslog message..."
                logger -n 127.0.0.1 -P 1514 -T "test message from monitor script"
                log_info "Test message sent. Check Grafana for logs."
                ;;
            17) 
                reset_grafana_password
                ;;
            18)
                create_backup
                ;;
            19)
                list_backups
                ;;
            20)
                list_backups
                echo
                read -rp "Enter backup name to restore: " backup_name
                if [ -n "$backup_name" ]; then
                    restore_backup "$backup_name"
                fi
                ;;
            21) 
                log_info "Exiting debug menu"
                break
                ;;
            *) log_warn "Invalid option. Please select 1-21." ;;
        esac
    done
}

reset_grafana_password() {
    local new_password="${1:-}"
    
    if [ -z "$new_password" ]; then
        if [ -f "${ENVFILE}" ]; then
            source "${ENVFILE}"
            new_password="${GRAFANA_ADMIN_PASSWORD:-}"
        fi
        
        if [ -z "$new_password" ]; then
            log_error "No password provided and GRAFANA_ADMIN_PASSWORD not found in .env"
            log_info "Usage: $0 reset-password <new_password>"
            log_info "Or set GRAFANA_ADMIN_PASSWORD in ${ENVFILE}"
            exit 1
        fi
    fi
    
    log_header "Resetting Grafana Admin Password"
    
    local container=$(get_container_name grafana)
    local status=$(get_container_status "$container")
    
    if [ "$status" != "running" ]; then
        log_error "Grafana container is not running"
        exit 1
    fi
    
    log_info "Resetting admin password..."
    if docker exec "$container" grafana-cli admin reset-admin-password "$new_password" 2>/dev/null; then
        log_info "✓ Grafana admin password reset successfully"
        log_info "New password: ${new_password}"
    else
        log_error "Failed to reset password. Trying alternative method..."
        docker exec "$container" grafana-cli admin reset-admin-password "$new_password" --homepath /usr/share/grafana 2>&1 || {
            log_error "Password reset failed"
            log_info "You may need to reset via Grafana web UI or delete grafana-data volume"
            exit 1
        }
        log_info "✓ Grafana admin password reset successfully"
    fi
}

shell_into() {
    local container_type="$1"
    
    case "$container_type" in
        "loki")
            log_info "Opening shell in Loki container..."
            docker exec -it "$(get_container_name loki)" /bin/sh
            ;;
        "grafana")
            log_info "Opening shell in Grafana container..."
            docker exec -it "$(get_container_name grafana)" /bin/bash
            ;;
        "prometheus")
            log_info "Opening shell in Prometheus container..."
            docker exec -it "$(get_container_name prometheus)" /bin/sh
            ;;
        "rsyslog")
            log_info "Opening shell in rsyslog container..."
            docker exec -it "$(get_container_name rsyslog)" /bin/bash
            ;;
        "traefik")
            log_info "Opening shell in Traefik container..."
            docker exec -it "$(get_container_name traefik)" /bin/sh
            ;;
        *)
            log_error "Invalid container type: $container_type"
            log_info "Valid options: loki, grafana, prometheus, rsyslog, traefik"
            exit 1
            ;;
    esac
}

check_smtp_config() {
    log_header "SMTP Configuration Check"
    
    if [ ! -f "${ENVFILE}" ]; then
        log_error "Environment file not found: ${ENVFILE}"
        exit 1
    fi
    
    log_info "Reading SMTP configuration from ${ENVFILE}..."
    echo
    
    local smtp_enabled=$(grep "^SMTP_ENABLED=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local smtp_host=$(grep "^SMTP_HOST=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local smtp_port=$(grep "^SMTP_PORT=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local smtp_user=$(grep "^SMTP_USER=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local alert_email_from=$(grep "^ALERT_EMAIL_FROM=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local alert_email_to=$(grep "^ALERT_EMAIL_TO=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    local smtp_skip_verify=$(grep "^SMTP_SKIP_VERIFY=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2 || echo "")
    
    if [ -z "$smtp_enabled" ] || [ "$smtp_enabled" != "true" ]; then
        log_warn "SMTP is not enabled (SMTP_ENABLED=${smtp_enabled:-not set})"
        return 1
    fi
    
    log_info "SMTP Status: ${GREEN}ENABLED${NC}"
    echo
    echo "Configuration:"
    echo "  SMTP Host:     ${smtp_host:-not set}"
    echo "  SMTP Port:     ${smtp_port:-not set}"
    echo "  SMTP User:     ${smtp_user:-not set}"
    echo "  From Address:  ${alert_email_from:-not set}"
    echo "  To Address:    ${alert_email_to:-not set}"
    echo "  Skip Verify:   ${smtp_skip_verify:-not set}"
    
    echo
    log_info "Checking Grafana container for SMTP environment variables..."
    local container=$(get_container_name grafana)
    local status=$(get_container_status "$container")
    
    if [ "$status" = "running" ]; then
        echo
        echo "Grafana Container Environment:"
        docker exec "$container" env | grep "^GF_SMTP" | sed 's/=.*/=***/' || log_warn "No SMTP environment variables found in container"
    else
        log_warn "Grafana container is not running"
        return 1
    fi
    
    echo
    log_header "Checking Grafana Logs for SMTP Activity"
    cd "${BASE}"
    
    log_info "Searching for SMTP-related log entries (last 200 lines)..."
    echo
    
    local smtp_logs=$(docker compose logs --tail=200 grafana 2>/dev/null | grep -iE "(smtp|email|mail|alert.*send|notification.*send)" || echo "")
    
    if [ -z "$smtp_logs" ]; then
        log_warn "No SMTP-related log entries found in recent Grafana logs"
        log_info "This could mean:"
        log_info "  - SMTP has not been used yet (no alerts sent)"
        log_info "  - SMTP connection attempts are failing silently"
        log_info "  - Logs are at a different log level"
    else
        log_info "Found SMTP-related log entries:"
        echo "----------------------------------------"
        echo "$smtp_logs"
        echo "----------------------------------------"
    fi
    
    echo
    log_info "Checking for SMTP connection attempts to port ${smtp_port:-unknown}..."
    
    if [ -n "$smtp_port" ] && [ -n "$smtp_host" ]; then
        local host_only=$(echo "$smtp_host" | cut -d':' -f1)
        log_info "Looking for connections to ${host_only}:${smtp_port}..."
        
        local connection_logs=$(docker compose logs --tail=500 grafana 2>/dev/null | grep -iE "(connect.*${host_only}|${smtp_port}|dial.*tcp.*${smtp_port})" || echo "")
        
        if [ -n "$connection_logs" ]; then
            echo "Found connection attempts:"
            echo "$connection_logs" | tail -10
        else
            log_warn "No connection attempts to ${host_only}:${smtp_port} found in logs"
        fi
    fi
    
    echo
    log_info "Recent Grafana error/warning logs (last 50 lines):"
    echo "----------------------------------------"
    docker compose logs --tail=50 grafana 2>/dev/null | grep -iE "(error|warn|fail)" | tail -10 || log_info "No errors/warnings found"
    echo "----------------------------------------"
    
    echo
    log_info "To test SMTP, configure a notification channel in Grafana UI:"
    log_info "  Alerting > Contact points > Add contact point > Test"
}

create_backup() {
    local backup_name="${1:-}"
    local timestamp=$(date +%Y%m%d_%H%M%S)
    
    if [ -z "$backup_name" ]; then
        backup_name="rosi-collector-backup-${timestamp}"
    fi
    
    local backup_path="${BACKUP_DIR}/${backup_name}"
    
    log_header "Creating Backup: ${backup_name}"
    
    mkdir -p "${BACKUP_DIR}"
    mkdir -p "${backup_path}"
    
    cd "${BASE}"
    
    # Check if services are running
    local services_running=false
    if docker compose ps --status running 2>/dev/null | grep -q "running"; then
        services_running=true
    fi
    
    # Backup configuration files
    log_info "Backing up configuration files..."
    mkdir -p "${backup_path}/config"
    cp -a docker-compose.yml "${backup_path}/config/" 2>/dev/null || true
    cp -a .env "${backup_path}/config/" 2>/dev/null || true
    cp -a loki-config.yml "${backup_path}/config/" 2>/dev/null || true
    cp -a prometheus.yml "${backup_path}/config/" 2>/dev/null || true
    cp -a -r prometheus-targets "${backup_path}/config/" 2>/dev/null || true
    cp -a -r rsyslog.conf "${backup_path}/config/" 2>/dev/null || true
    cp -a -r traefik "${backup_path}/config/" 2>/dev/null || true
    cp -a -r grafana/provisioning "${backup_path}/config/grafana-provisioning" 2>/dev/null || true
    log_info "Configuration files backed up"
    
    # Backup Docker volumes
    log_info "Backing up Docker volumes..."
    
    # Grafana volume
    log_info "  - Backing up Grafana data..."
    if docker volume inspect rosi-collector_grafana-data >/dev/null 2>&1; then
        docker run --rm \
            -v rosi-collector_grafana-data:/source:ro \
            -v "${backup_path}:/backup" \
            alpine tar czf /backup/grafana-data.tar.gz -C /source .
        log_info "    Grafana data backed up"
    else
        log_warn "    Grafana volume not found, skipping"
    fi
    
    # Loki volume
    log_info "  - Backing up Loki data..."
    if docker volume inspect rosi-collector_loki-data >/dev/null 2>&1; then
        docker run --rm \
            -v rosi-collector_loki-data:/source:ro \
            -v "${backup_path}:/backup" \
            alpine tar czf /backup/loki-data.tar.gz -C /source .
        log_info "    Loki data backed up"
    else
        log_warn "    Loki volume not found, skipping"
    fi
    
    # Prometheus volume
    log_info "  - Backing up Prometheus data..."
    if docker volume inspect rosi-collector_prometheus-data >/dev/null 2>&1; then
        docker run --rm \
            -v rosi-collector_prometheus-data:/source:ro \
            -v "${backup_path}:/backup" \
            alpine tar czf /backup/prometheus-data.tar.gz -C /source .
        log_info "    Prometheus data backed up"
    else
        log_warn "    Prometheus volume not found, skipping"
    fi
    
    # Create backup manifest
    log_info "Creating backup manifest..."
    cat > "${backup_path}/manifest.txt" <<EOF
Backup: ${backup_name}
Created: $(date -Iseconds)
Host: $(hostname)
Services running at backup: ${services_running}

Contents:
$(ls -la "${backup_path}")

Docker volumes backed up:
$(docker volume ls --format "{{.Name}}" | grep "^rosi-collector_" || echo "none found")
EOF
    
    # Calculate backup size
    local backup_size=$(du -sh "${backup_path}" | cut -f1)
    
    echo
    log_info "Backup completed successfully!"
    log_info "Location: ${backup_path}"
    log_info "Size: ${backup_size}"
    echo
    log_info "Contents:"
    ls -lh "${backup_path}"
}

restore_backup() {
    local backup_name="${1:-}"
    
    if [ -z "$backup_name" ]; then
        log_header "Available Backups"
        if [ -d "${BACKUP_DIR}" ]; then
            ls -lt "${BACKUP_DIR}" 2>/dev/null || log_warn "No backups found"
        else
            log_warn "Backup directory does not exist: ${BACKUP_DIR}"
        fi
        echo
        log_info "Usage: $0 restore <backup-name>"
        return 1
    fi
    
    local backup_path="${BACKUP_DIR}/${backup_name}"
    
    if [ ! -d "${backup_path}" ]; then
        log_error "Backup not found: ${backup_path}"
        log_info "Available backups:"
        ls -lt "${BACKUP_DIR}" 2>/dev/null || log_warn "No backups found"
        return 1
    fi
    
    log_header "Restoring Backup: ${backup_name}"
    
    if [ -f "${backup_path}/manifest.txt" ]; then
        log_info "Backup manifest:"
        cat "${backup_path}/manifest.txt"
        echo
    fi
    
    read -rp "This will stop services and restore data. Continue? (y/N): " confirm
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        log_info "Restore cancelled"
        return 0
    fi
    
    cd "${BASE}"
    
    # Stop services
    log_info "Stopping services..."
    docker compose down 2>/dev/null || true
    
    # Restore configuration files
    if [ -d "${backup_path}/config" ]; then
        log_info "Restoring configuration files..."
        cp -a "${backup_path}/config/docker-compose.yml" . 2>/dev/null || true
        cp -a "${backup_path}/config/.env" . 2>/dev/null || true
        cp -a "${backup_path}/config/loki-config.yml" . 2>/dev/null || true
        cp -a "${backup_path}/config/prometheus.yml" . 2>/dev/null || true
        cp -a -r "${backup_path}/config/prometheus-targets" . 2>/dev/null || true
        cp -a -r "${backup_path}/config/rsyslog.conf" . 2>/dev/null || true
        cp -a -r "${backup_path}/config/traefik" . 2>/dev/null || true
        if [ -d "${backup_path}/config/grafana-provisioning" ]; then
            mkdir -p grafana
            cp -a -r "${backup_path}/config/grafana-provisioning" grafana/provisioning 2>/dev/null || true
        fi
        log_info "Configuration files restored"
    fi
    
    # Restore Docker volumes
    log_info "Restoring Docker volumes..."
    
    # Grafana volume
    if [ -f "${backup_path}/grafana-data.tar.gz" ]; then
        log_info "  - Restoring Grafana data..."
        docker volume rm rosi-collector_grafana-data 2>/dev/null || true
        docker volume create rosi-collector_grafana-data
        docker run --rm \
            -v rosi-collector_grafana-data:/target \
            -v "${backup_path}:/backup:ro" \
            alpine sh -c "cd /target && tar xzf /backup/grafana-data.tar.gz"
        log_info "    Grafana data restored"
    fi
    
    # Loki volume
    if [ -f "${backup_path}/loki-data.tar.gz" ]; then
        log_info "  - Restoring Loki data..."
        docker volume rm rosi-collector_loki-data 2>/dev/null || true
        docker volume create rosi-collector_loki-data
        docker run --rm \
            -v rosi-collector_loki-data:/target \
            -v "${backup_path}:/backup:ro" \
            alpine sh -c "cd /target && tar xzf /backup/loki-data.tar.gz"
        log_info "    Loki data restored"
    fi
    
    # Prometheus volume
    if [ -f "${backup_path}/prometheus-data.tar.gz" ]; then
        log_info "  - Restoring Prometheus data..."
        docker volume rm rosi-collector_prometheus-data 2>/dev/null || true
        docker volume create rosi-collector_prometheus-data
        docker run --rm \
            -v rosi-collector_prometheus-data:/target \
            -v "${backup_path}:/backup:ro" \
            alpine sh -c "cd /target && tar xzf /backup/prometheus-data.tar.gz"
        log_info "    Prometheus data restored"
    fi
    
    echo
    log_info "Restore completed!"
    log_info "Start services with: cd ${BASE} && docker compose up -d"
}

list_backups() {
    log_header "Available Backups"
    
    if [ ! -d "${BACKUP_DIR}" ]; then
        log_warn "Backup directory does not exist: ${BACKUP_DIR}"
        return 0
    fi
    
    local backups=$(ls -1 "${BACKUP_DIR}" 2>/dev/null)
    
    if [ -z "$backups" ]; then
        log_info "No backups found in ${BACKUP_DIR}"
        return 0
    fi
    
    echo
    printf "%-40s %10s %s\n" "BACKUP NAME" "SIZE" "DATE"
    echo "--------------------------------------------------------------------------------"
    
    for backup in $backups; do
        local backup_path="${BACKUP_DIR}/${backup}"
        if [ -d "$backup_path" ]; then
            local size=$(du -sh "$backup_path" 2>/dev/null | cut -f1)
            local date=$(stat -c %y "$backup_path" 2>/dev/null | cut -d'.' -f1)
            printf "%-40s %10s %s\n" "$backup" "$size" "$date"
        fi
    done
    
    echo
    log_info "Backup location: ${BACKUP_DIR}"
}

reset_grafana_password() {
    local new_password="${1:-}"
    
    log_header "Reset Grafana Admin Password"
    
    # If no password provided, read from .env
    if [ -z "${new_password}" ]; then
        if [ -f "${ENVFILE}" ]; then
            new_password=$(grep "^GRAFANA_ADMIN_PASSWORD=" "${ENVFILE}" | cut -d'=' -f2- | tr -d '"' | tr -d "'")
            if [ -n "${new_password}" ]; then
                log_info "Using password from .env file"
            else
                log_error "GRAFANA_ADMIN_PASSWORD not found in ${ENVFILE}"
                log_info "Usage: $0 reset-password [NEW_PASSWORD]"
                return 1
            fi
        else
            log_error ".env file not found: ${ENVFILE}"
            log_info "Usage: $0 reset-password NEW_PASSWORD"
            return 1
        fi
    fi
    
    # Check if Grafana container is running
    local grafana_container=$(get_container_name "grafana")
    local status=$(get_container_status "$grafana_container")
    
    if [ "$status" != "running" ]; then
        log_error "Grafana container is not running: ${grafana_container}"
        log_info "Start the stack first: cd ${BASE} && docker compose up -d"
        return 1
    fi
    
    # Reset the password using grafana-cli
    log_info "Resetting admin password..."
    if docker exec "${grafana_container}" grafana-cli admin reset-admin-password "${new_password}" 2>/dev/null; then
        log_info "✓ Password reset successfully!"
        echo
        log_info "Login credentials:"
        log_info "  URL:      https://$(grep "^TRAEFIK_DOMAIN=" "${ENVFILE}" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || echo 'your-domain')/"
        log_info "  Username: admin"
        log_info "  Password: ${new_password}"
    else
        log_error "Failed to reset password"
        log_info "Try manually: docker exec -it ${grafana_container} grafana-cli admin reset-admin-password YOUR_PASSWORD"
        return 1
    fi
}

show_usage() {
    echo "Usage: $0 <command> [options]"
    echo
    echo "Commands:"
    echo "  status                    Show container status and resource usage"
    echo "  logs [lines] [-f]        Show recent logs (default: 50 lines, -f to follow)"
    echo "  health                    Check health of all services"
    echo "  smtp                      Check SMTP configuration from .env"
    echo "  backup [name]             Create backup (optional: custom name)"
    echo "  restore [name]            Restore from backup (list backups if no name)"
    echo "  backups                   List available backups"
    echo "  debug                     Interactive debugging menu"
    echo "  shell <container>         Shell into container (loki|grafana|prometheus|rsyslog|traefik)"
    echo "  reset-password [pass]     Reset Grafana admin password (uses .env if not provided)"
    echo "  help                      Show this help message"
    echo
    echo "Examples:"
    echo "  $0 status"
    echo "  $0 logs 100"
    echo "  $0 logs -f                # Follow logs (real-time)"
    echo "  $0 logs 50 -f             # Show last 50 lines then follow"
    echo "  $0 health"
    echo "  $0 smtp                   # Check SMTP configuration"
    echo "  $0 backup                 # Create backup with timestamp"
    echo "  $0 backup my-backup       # Create backup with custom name"
    echo "  $0 backups                # List all backups"
    echo "  $0 restore                # List available backups"
    echo "  $0 restore my-backup      # Restore from specific backup"
    echo "  $0 debug"
    echo "  $0 shell loki"
    echo "  $0 shell grafana"
    echo "  $0 reset-password mynewpass"
    echo "  $0 reset-password           # Uses GRAFANA_ADMIN_PASSWORD from .env"
}

main() {
    local command="${1:-help}"
    
    check_privileges
    check_docker
    check_compose
    
    case "$command" in
        "status")
            show_status
            ;;
        "logs")
            local lines="50"
            local follow="false"
            # Parse arguments: logs [-f] [lines]
            shift  # Remove 'logs' from arguments
            while [ $# -gt 0 ]; do
                case "$1" in
                    -f) follow="true" ;;
                    [0-9]*) lines="$1" ;;
                esac
                shift
            done
            show_logs "$lines" "$follow"
            ;;
        "health")
            check_health
            ;;
        "smtp")
            check_smtp_config
            ;;
        "backup")
            create_backup "${2:-}"
            ;;
        "restore")
            restore_backup "${2:-}"
            ;;
        "backups"|"list-backups")
            list_backups
            ;;
        "debug")
            debug_menu
            ;;
        "shell")
            shell_into "${2:-}"
            ;;
        "reset-password")
            reset_grafana_password "${2:-}"
            ;;
        "help"|"--help"|"-h")
            show_usage
            ;;
        *)
            log_error "Unknown command: $command"
            echo
            show_usage
            exit 1
            ;;
    esac
}

main "$@"

