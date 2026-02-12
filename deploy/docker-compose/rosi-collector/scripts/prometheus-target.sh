#!/usr/bin/env bash
## @file prometheus-target.sh
## @brief Manage Prometheus scrape targets for ROSI.
##
## Installed to /usr/local/bin/prometheus-target by init.sh

set -euo pipefail

# Detect INSTALL_DIR from multiple sources
detect_install_dir() {
  # 1. Environment variable takes precedence
  if [ -n "${INSTALL_DIR:-}" ]; then
    echo "${INSTALL_DIR}"
    return 0
  fi
  
  # 2. Check user config file
  local user_config="${HOME}/.config/rsyslog/rosi-collector.conf"
  if [ -f "$user_config" ]; then
    local dir
    dir=$(grep "^INSTALL_DIR=" "$user_config" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || true)
    if [ -n "$dir" ] && [ -d "$dir" ]; then
      echo "$dir"
      return 0
    fi
  fi
  
  # 3. Check system config file
  local system_config="/etc/rsyslog/rosi-collector.conf"
  if [ -f "$system_config" ]; then
    local dir
    dir=$(grep "^INSTALL_DIR=" "$system_config" 2>/dev/null | cut -d'=' -f2- | tr -d '"' || true)
    if [ -n "$dir" ] && [ -d "$dir" ]; then
      echo "$dir"
      return 0
    fi
  fi
  
  # 4. Check common locations
  for dir in /opt/rosi-collector /srv/central /srv/rosi-collector; do
    if [ -d "$dir/prometheus-targets" ]; then
      echo "$dir"
      return 0
    fi
  done
  
  # 5. Default fallback
  echo "/opt/rosi-collector"
}

INSTALL_DIR="$(detect_install_dir)"
JOB="node"
TARGETS_FILE=""

usage() {
  cat <<EOF
Usage: $(basename "$0") [--job node|impstats] <command> [options]

Options:
  --job <name>                Select target job (node|impstats)
  --targets-file <path>       Override target file path

Commands:
  add <ip:port> [labels...]   Add a new target
  add-client <ip> [labels...] Add node + impstats targets for a host
  remove <ip:port|hostname>   Remove a target by IP:port or hostname
  list                        List all targets
  
Labels (for add command):
  host=<name>                 Hostname label (required)
  role=<value>                Role label (e.g., web, db, monitoring)
  network=<value>             Network label (e.g., internal, external)
  env=<value>                 Environment label (e.g., production, staging)
  <key>=<value>               Any custom label

Examples:
  $(basename "$0") add 10.0.0.5:9100 host=webserver role=web network=internal
  $(basename "$0") add 192.168.1.10:9100 host=database role=db env=production
  $(basename "$0") --job impstats add 127.0.0.1:9898 host=rosi-collector role=rsyslog
  $(basename "$0") add-client 10.0.0.5 host=webserver role=web network=internal
  $(basename "$0") remove 10.0.0.5:9100
  $(basename "$0") remove webserver
  $(basename "$0") list

Environment:
  INSTALL_DIR                 Override installation directory
                              (default: auto-detected or /opt/rosi-collector)
EOF
  exit 1
}

resolve_targets_file() {
  if [ -n "$TARGETS_FILE" ]; then
    return 0
  fi

  case "$JOB" in
    node)
      TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/nodes.yml"
      ;;
    impstats)
      TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/impstats.yml"
      ;;
    *)
      echo "Error: Unknown job '$JOB' (use node or impstats)" >&2
      exit 1
      ;;
  esac
}

check_root() {
  if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "Error: Run as root" >&2
    exit 1
  fi
}

check_file() {
  if [ ! -f "$TARGETS_FILE" ]; then
    echo "Error: Targets file not found: $TARGETS_FILE" >&2
    echo "Run init.sh first to create the ROSI Collector stack" >&2
    echo "" >&2
    echo "Or set INSTALL_DIR environment variable to your installation path:" >&2
    echo "  INSTALL_DIR=/srv/central prometheus-target list" >&2
    echo "If managing impstats targets, use:" >&2
    echo "  prometheus-target --job impstats list" >&2
    exit 1
  fi
}

validate_target() {
  local target="$1"
  if [[ ! "$target" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+:[0-9]+$ ]]; then
    echo "Error: Invalid target format. Use IP:PORT (e.g., 10.0.0.1:9100)" >&2
    exit 1
  fi
}

validate_ip() {
  local ip="$1"
  if [[ ! "$ip" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    return 1
  fi
  local a b c d
  IFS='.' read -r a b c d <<<"$ip"
  for octet in "$a" "$b" "$c" "$d"; do
    if [ "$octet" -lt 0 ] || [ "$octet" -gt 255 ]; then
      return 1
    fi
  done
  return 0
}

ensure_host_label() {
  local has_host="false"
  for arg in "$@"; do
    if [[ "$arg" =~ ^([a-zA-Z_][a-zA-Z0-9_]*)=(.+)$ ]]; then
      local key="${BASH_REMATCH[1]}"
      if [ "$key" = "host" ]; then
        has_host="true"
      fi
    else
      echo "Error: Invalid label format: $arg (use key=value)" >&2
      exit 1
    fi
  done
  if [ "$has_host" != "true" ]; then
    echo "Error: host=<name> label is required" >&2
    exit 1
  fi
}

target_exists() {
  local target="$1"
  grep -q "^\s*-\s*${target}$" "$TARGETS_FILE" 2>/dev/null
}

cmd_add() {
  if [ $# -lt 2 ]; then
    echo "Error: Missing target and/or host label" >&2
    echo "Usage: $(basename "$0") add <ip:port> host=<name> [labels...]" >&2
    exit 1
  fi

  local target="$1"
  shift
  validate_target "$target"

  if target_exists "$target"; then
    echo "Error: Target $target already exists" >&2
    exit 1
  fi

  local host=""
  local labels=()
  
  for arg in "$@"; do
    if [[ "$arg" =~ ^([a-zA-Z_][a-zA-Z0-9_]*)=(.+)$ ]]; then
      local key="${BASH_REMATCH[1]}"
      local value="${BASH_REMATCH[2]}"
      if [ "$key" = "host" ]; then
        host="$value"
      fi
      labels+=("$key: $value")
    else
      echo "Error: Invalid label format: $arg (use key=value)" >&2
      exit 1
    fi
  done

  if [ -z "$host" ]; then
    echo "Error: host=<name> label is required" >&2
    exit 1
  fi

  {
    echo ""
    echo "# $host"
    echo "- targets:"
    echo "    - $target"
    echo "  labels:"
    for label in "${labels[@]}"; do
      echo "    $label"
    done
  } >> "$TARGETS_FILE"

  # Ensure Prometheus container can read the file (runs as nobody/65534)
  chmod 644 "$TARGETS_FILE"

  echo "Added target: $target (host=$host)"
  echo "Prometheus will pick up changes within 5 minutes"
}

cmd_add_client() {
  if [ $# -lt 2 ]; then
    echo "Error: Missing IP and/or host label" >&2
    echo "Usage: $(basename "$0") add-client <ip> host=<name> [labels...]" >&2
    exit 1
  fi

  local ip="$1"
  shift

  if ! validate_ip "$ip"; then
    echo "Error: Invalid IP address: $ip" >&2
    exit 1
  fi

  ensure_host_label "$@"

  # Pre-check both targets so we don't leave partial config if the second add fails
  TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/nodes.yml"
  check_file
  if target_exists "${ip}:9100"; then
    echo "Error: Target ${ip}:9100 already exists in nodes" >&2
    exit 1
  fi
  TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/impstats.yml"
  check_file
  if target_exists "${ip}:9898"; then
    echo "Error: Target ${ip}:9898 already exists in impstats" >&2
    exit 1
  fi

  TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/nodes.yml"
  cmd_add "${ip}:9100" "$@"

  TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/impstats.yml"
  ( cmd_add "${ip}:9898" "$@" ) || {
    echo "Error: Failed to add impstats target; rolling back node target" >&2
    TARGETS_FILE="${INSTALL_DIR}/prometheus-targets/nodes.yml"
    cmd_remove "${ip}:9100"
    exit 1
  }
}

cmd_remove() {
  if [ $# -lt 1 ]; then
    echo "Error: Missing target or hostname" >&2
    echo "Usage: $(basename "$0") remove <ip:port|hostname>" >&2
    exit 1
  fi

  local search="$1"
  local target=""
  local by_host=false
  
  # Check if it's an IP:port or a hostname
  if [[ "$search" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+:[0-9]+$ ]]; then
    target="$search"
    if ! target_exists "$target"; then
      echo "Error: Target $target not found" >&2
      exit 1
    fi
  else
    # Assume it's a hostname - find the corresponding target
    by_host=true
    target=$(awk -v host="$search" '
      /^    - [0-9]/ { current_target = $2 }
      /host:/ && $2 == host { print current_target; exit }
    ' "$TARGETS_FILE")
    
    if [ -z "$target" ]; then
      echo "Error: Host '$search' not found in targets" >&2
      exit 1
    fi
  fi

  local tmpfile
  tmpfile=$(mktemp)
  
  awk -v target="$target" '
    BEGIN { skip = 0; blank_count = 0 }
    /^#/ && skip == 0 { comment = $0; next }
    /^- targets:/ { 
      if (skip == 0) { 
        block_start = NR
        block_comment = comment
        comment = ""
      }
    }
    /^    - / {
      if ($2 == target) {
        skip = 1
        next
      }
    }
    /^  labels:/ && skip == 1 { next }
    /^    [a-z]/ && skip == 1 { next }
    /^$/ {
      if (skip == 1) { skip = 0; next }
      blank_count++
      if (blank_count > 1) next
    }
    /^[^ #]/ || /^- targets:/ { 
      if (skip == 1) skip = 0
      blank_count = 0
    }
    skip == 0 { 
      if (comment != "") { print comment; comment = "" }
      print 
    }
  ' "$TARGETS_FILE" > "$tmpfile"

  mv "$tmpfile" "$TARGETS_FILE"
  chmod 644 "$TARGETS_FILE"

  echo "Removed target: $target"
  echo "Prometheus will pick up changes within 5 minutes"
}

cmd_list() {
  echo "Prometheus Targets (${JOB})"
  echo "================================="
  echo ""
  
  if ! grep -q "^- targets:" "$TARGETS_FILE" 2>/dev/null; then
    echo "(no targets configured)"
    return
  fi

  awk '
    function print_entry() {
      if (target != "") {
        printf "%-24s  host=%-25s", target, host
        if (role != "") printf "  role=%-12s", role
        if (network != "") printf "  network=%s", network
        printf "\n"
      }
    }
    /^- targets:/ { 
      print_entry()
      target = ""; host = ""; role = ""; network = ""
      in_block = 1
    }
    /^    - [0-9]/ && in_block { target = $2 }
    /^    host:/ && in_block { host = $2 }
    /^    role:/ && in_block { role = $2 }
    /^    network:/ && in_block { network = $2 }
    END { print_entry() }
  ' "$TARGETS_FILE"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --job)
      if [ $# -lt 2 ]; then
        echo "Error: --job requires a value" >&2
        exit 1
      fi
      JOB="$2"
      shift 2
      ;;
    --targets-file)
      if [ $# -lt 2 ]; then
        echo "Error: --targets-file requires a value" >&2
        exit 1
      fi
      TARGETS_FILE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

check_root

case "${1:-}" in
  add)
    shift
    resolve_targets_file
    check_file
    cmd_add "$@"
    ;;
  add-client)
    shift
    cmd_add_client "$@"
    ;;
  remove|rm|delete)
    shift
    resolve_targets_file
    check_file
    cmd_remove "$@"
    ;;
  list|ls)
    resolve_targets_file
    check_file
    cmd_list
    ;;
  *)
    usage
    ;;
esac
