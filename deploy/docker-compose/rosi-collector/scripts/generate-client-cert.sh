#!/usr/bin/env bash
## @file generate-client-cert.sh
## @brief Generate client certificates for rsyslog TLS with secure download
## @description
##   Creates client certificates signed by the local CA and optionally
##   generates a secure one-time download token for distribution.
##
## @usage
##   ./generate-client-cert.sh [OPTIONS] CLIENT_NAME
##
## @option -d, --dir DIR         Certificate directory (default: ../certs)
## @option --days DAYS           Certificate validity (default: 365)
## @option --download            Generate one-time download package
## @option --token-validity SEC  Download token validity in seconds (default: 300)
## @option -l, --list            List existing client certificates
## @option --revoke NAME         Revoke a client certificate
## @option --help                Show this help

set -euo pipefail

# Defaults
CERT_DIR=""
CLIENT_DAYS=365
GENERATE_DOWNLOAD=false
TOKEN_VALIDITY=300
LIST_CERTS=false
REVOKE_NAME=""
CLIENT_NAME=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Determine ROSI base directory
# Priority: ROSI_HOME env var > /srv/central (installed) > script's parent dir (dev)
if [[ -n "${ROSI_HOME:-}" ]]; then
    ROSI_BASE="$ROSI_HOME"
elif [[ -f "/srv/central/.env" ]]; then
    ROSI_BASE="/srv/central"
elif [[ -f "${SCRIPT_DIR}/../.env" ]]; then
    ROSI_BASE="${SCRIPT_DIR}/.."
else
    # Fallback to /srv/central even if .env doesn't exist yet
    ROSI_BASE="/srv/central"
fi

# Load environment if available
if [[ -f "${ROSI_BASE}/.env" ]]; then
    # shellcheck source=/dev/null
    source "${ROSI_BASE}/.env" 2>/dev/null || true
fi

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS] CLIENT_NAME

Generate client certificates for rsyslog TLS authentication.

Arguments:
  CLIENT_NAME         Unique name for the client (e.g., hostname or identifier)

Options:
  -d, --dir DIR         Certificate directory (default: /srv/central/certs)
  --days DAYS           Certificate validity in days (default: 365)
  --download            Generate secure one-time download package
  --token-validity SEC  Download token validity in seconds (default: 300 = 5 min)
  -l, --list            List existing client certificates
  --revoke NAME         Revoke a client certificate (not implemented yet)
  --help                Show this help message

Environment:
  ROSI_HOME             Override base directory (default: /srv/central)

Examples:
  $(basename "$0") webserver01
  $(basename "$0") --download webserver01
  $(basename "$0") -l

The --download option creates a tarball with:
  - ca.pem (CA certificate)
  - client-cert.pem (client certificate)
  - client-key.pem (client private key)
  - rsyslog-tls.conf (ready-to-use rsyslog config snippet)

The package is placed in downloads/tls-packages/ with a one-time token filename.
After download, the token file should be deleted.

EOF
    exit 0
}

log_info() { echo "[INFO] $*"; }
log_warn() { echo "[WARN] $*" >&2; }
log_error() { echo "[ERROR] $*" >&2; }

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--dir)
            CERT_DIR="$2"
            shift 2
            ;;
        --days)
            CLIENT_DAYS="$2"
            shift 2
            ;;
        --download)
            GENERATE_DOWNLOAD=true
            shift
            ;;
        --token-validity)
            TOKEN_VALIDITY="$2"
            shift 2
            ;;
        -l|--list)
            LIST_CERTS=true
            shift
            ;;
        --revoke)
            REVOKE_NAME="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            exit 1
            ;;
        *)
            CLIENT_NAME="$1"
            shift
            ;;
    esac
done

# Set default cert dir
if [[ -z "$CERT_DIR" ]]; then
    CERT_DIR="${ROSI_BASE}/certs"
fi
mkdir -p "$CERT_DIR"
CERT_DIR="$(cd "$CERT_DIR" && pwd)"

# Handle list mode
if [[ "$LIST_CERTS" == "true" ]]; then
    log_info "Client certificates in $CERT_DIR/clients/:"
    if [[ -d "$CERT_DIR/clients" ]]; then
        for cert in "$CERT_DIR/clients"/*-cert.pem; do
            [[ -f "$cert" ]] || continue
            name=$(basename "$cert" | sed 's/-cert\.pem$//')
            expiry=$(openssl x509 -in "$cert" -noout -enddate 2>/dev/null | cut -d= -f2)
            echo "  $name (expires: $expiry)"
        done
    else
        echo "  (none)"
    fi
    exit 0
fi

# Handle revoke mode (placeholder)
if [[ -n "$REVOKE_NAME" ]]; then
    log_error "Certificate revocation not yet implemented"
    log_info "To revoke, manually remove: $CERT_DIR/clients/${REVOKE_NAME}-*.pem"
    exit 1
fi

# Client name is required for generation
if [[ -z "$CLIENT_NAME" ]]; then
    log_error "Client name is required"
    echo "Usage: $(basename "$0") [OPTIONS] CLIENT_NAME"
    echo "Try '$(basename "$0") --help' for more information."
    exit 1
fi

# Validate client name (alphanumeric, dash, underscore only)
if [[ ! "$CLIENT_NAME" =~ ^[a-zA-Z0-9_-]+$ ]]; then
    log_error "Client name must be alphanumeric (dashes and underscores allowed)"
    exit 1
fi

# Check CA exists
if [[ ! -f "$CERT_DIR/ca.pem" || ! -f "$CERT_DIR/ca-key.pem" ]]; then
    log_error "CA certificate not found. Run generate-ca.sh first."
    exit 1
fi

# Create clients directory
CLIENTS_DIR="$CERT_DIR/clients"
mkdir -p "$CLIENTS_DIR"

# Check if client cert already exists
if [[ -f "$CLIENTS_DIR/${CLIENT_NAME}-cert.pem" ]]; then
    log_warn "Certificate for '$CLIENT_NAME' already exists"
    read -rp "Regenerate? [y/N]: " confirm
    if [[ ! "$confirm" =~ ^[Yy] ]]; then
        log_info "Cancelled"
        exit 0
    fi
fi

log_info "Generating certificate for client: $CLIENT_NAME"

# Generate client private key
openssl genrsa -out "$CLIENTS_DIR/${CLIENT_NAME}-key.pem" 2048 2>/dev/null

# Generate client CSR
openssl req -new \
    -key "$CLIENTS_DIR/${CLIENT_NAME}-key.pem" \
    -out "$CLIENTS_DIR/${CLIENT_NAME}.csr" \
    -subj "/CN=${CLIENT_NAME}/O=rsyslog/OU=TLS Client" \
    2>/dev/null

# Sign with CA
openssl x509 -req \
    -days "$CLIENT_DAYS" \
    -in "$CLIENTS_DIR/${CLIENT_NAME}.csr" \
    -CA "$CERT_DIR/ca.pem" \
    -CAkey "$CERT_DIR/ca-key.pem" \
    -CAcreateserial \
    -out "$CLIENTS_DIR/${CLIENT_NAME}-cert.pem" \
    2>/dev/null

# Set permissions
chmod 600 "$CLIENTS_DIR/${CLIENT_NAME}-key.pem"
chmod 644 "$CLIENTS_DIR/${CLIENT_NAME}-cert.pem"

# Clean up CSR
rm -f "$CLIENTS_DIR/${CLIENT_NAME}.csr"

log_info "Client certificate created:"
log_info "  Certificate: $CLIENTS_DIR/${CLIENT_NAME}-cert.pem"
log_info "  Private Key: $CLIENTS_DIR/${CLIENT_NAME}-key.pem"
log_info "  Expires: $(openssl x509 -in "$CLIENTS_DIR/${CLIENT_NAME}-cert.pem" -noout -enddate | cut -d= -f2)"

# Generate download package if requested
if [[ "$GENERATE_DOWNLOAD" == "true" ]]; then
    log_info ""
    log_info "Creating secure download package..."
    
    # Get server hostname from environment or prompt
    SYSLOG_TLS_HOSTNAME="${SYSLOG_TLS_HOSTNAME:-${TRAEFIK_DOMAIN:-}}"
    if [[ -z "$SYSLOG_TLS_HOSTNAME" ]]; then
        read -rp "Enter collector hostname (e.g., logs.example.com): " SYSLOG_TLS_HOSTNAME
    fi
    
    # Create temporary directory for package
    TEMP_DIR=$(mktemp -d)
    PKG_DIR="$TEMP_DIR/${CLIENT_NAME}-tls"
    mkdir -p "$PKG_DIR"
    
    # Copy certificates
    cp "$CERT_DIR/ca.pem" "$PKG_DIR/"
    cp "$CLIENTS_DIR/${CLIENT_NAME}-cert.pem" "$PKG_DIR/client-cert.pem"
    cp "$CLIENTS_DIR/${CLIENT_NAME}-key.pem" "$PKG_DIR/client-key.pem"
    
    # Create rsyslog config snippet
    cat > "$PKG_DIR/rsyslog-tls.conf" << EOF
# Rsyslog TLS forwarding configuration
# Generated for: ${CLIENT_NAME}
# Date: $(date -Iseconds)
# 
# Installation:
#   1. Copy certificates to /etc/rsyslog.d/certs/
#   2. Copy this file to /etc/rsyslog.d/99-forward-tls.conf
#   3. Restart rsyslog: systemctl restart rsyslog
#
# Required package: rsyslog-openssl (apt install rsyslog-openssl)

# Global TLS settings
global(
    DefaultNetstreamDriver="ossl"
    DefaultNetstreamDriverCAFile="/etc/rsyslog.d/certs/ca.pem"
    DefaultNetstreamDriverCertFile="/etc/rsyslog.d/certs/client-cert.pem"
    DefaultNetstreamDriverKeyFile="/etc/rsyslog.d/certs/client-key.pem"
)

# Forward all logs via TLS to collector
*.* action(
    type="omfwd"
    target="${SYSLOG_TLS_HOSTNAME}"
    port="6514"
    protocol="tcp"
    StreamDriver="ossl"
    StreamDriverMode="1"
    StreamDriverAuthMode="x509/name"
    StreamDriverPermittedPeers="${SYSLOG_TLS_HOSTNAME}"
    # Disk-assisted queue for reliability
    queue.type="LinkedList"
    queue.size="10000"
    queue.filename="fwd_tls"
    queue.saveOnShutdown="on"
    action.resumeRetryCount="-1"
)
EOF

    # Create installation script
    cat > "$PKG_DIR/install.sh" << 'INSTALL_EOF'
#!/bin/bash
# TLS certificate installation script
set -euo pipefail

CERT_DEST="/etc/rsyslog.d/certs"
CONF_DEST="/etc/rsyslog.d"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check root
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root"
    exit 1
fi

# Detect rsyslog user (syslog on Debian/Ubuntu, root on RHEL/CentOS)
if id -u syslog >/dev/null 2>&1; then
    RSYSLOG_USER="syslog"
    RSYSLOG_GROUP="syslog"
elif id -u rsyslog >/dev/null 2>&1; then
    RSYSLOG_USER="rsyslog"
    RSYSLOG_GROUP="rsyslog"
else
    RSYSLOG_USER="root"
    RSYSLOG_GROUP="root"
fi

# Check rsyslog-openssl (Debian/Ubuntu)
if command -v dpkg >/dev/null 2>&1; then
    if ! dpkg -l rsyslog-openssl >/dev/null 2>&1; then
        echo "Installing rsyslog-openssl..."
        apt-get update && apt-get install -y rsyslog-openssl
    fi
fi

# Check for existing plain-text forwarding configs that might cause duplicate logs
echo ""
echo "Checking for existing forwarding configurations..."
PLAIN_FORWARD_CONFIGS=()
for conf in "$CONF_DEST"/*.conf; do
    [[ -f "$conf" ]] || continue
    # Skip the TLS config we're about to install
    [[ "$(basename "$conf")" == "99-forward-tls.conf" ]] && continue
    # Check if config contains omfwd actions (forwarding)
    if grep -qE '(type="omfwd"|type=.omfwd.|@@|@[^@])' "$conf" 2>/dev/null; then
        # Check if it's NOT a TLS config (no StreamDriver or ossl)
        if ! grep -qE '(StreamDriver|ossl|gtls|DefaultNetstreamDriver)' "$conf" 2>/dev/null; then
            PLAIN_FORWARD_CONFIGS+=("$conf")
        fi
    fi
done

if [[ ${#PLAIN_FORWARD_CONFIGS[@]} -gt 0 ]]; then
    echo ""
    echo "WARNING: Found existing plain-text forwarding configuration(s):"
    for conf in "${PLAIN_FORWARD_CONFIGS[@]}"; do
        echo "  - $conf"
    done
    echo ""
    echo "These configs will cause DUPLICATE logging if left enabled."
    echo "TLS forwarding replaces plain-text forwarding for security."
    echo ""
    read -rp "Disable these configs by renaming to .disabled? [Y/n]: " response
    response=${response:-Y}
    if [[ "$response" =~ ^[Yy] ]]; then
        for conf in "${PLAIN_FORWARD_CONFIGS[@]}"; do
            echo "  Disabling: $conf -> ${conf}.disabled"
            mv "$conf" "${conf}.disabled"
        done
        echo "Plain-text forwarding configs disabled."
        echo ""
    else
        echo ""
        echo "WARNING: Keeping plain-text configs. You may see duplicate logs!"
        echo "To fix later, remove or rename the plain-text forwarding configs."
        echo ""
    fi
fi

# Create certificate directory
mkdir -p "$CERT_DEST"

# Copy certificates
echo "Installing certificates to $CERT_DEST..."
cp "$SCRIPT_DIR/ca.pem" "$CERT_DEST/"
cp "$SCRIPT_DIR/client-cert.pem" "$CERT_DEST/"
cp "$SCRIPT_DIR/client-key.pem" "$CERT_DEST/"

# Set ownership and permissions
# rsyslog runs as syslog user on Debian/Ubuntu and needs to read the certs
chown -R "$RSYSLOG_USER:$RSYSLOG_GROUP" "$CERT_DEST"
chmod 755 "$CERT_DEST"
chmod 644 "$CERT_DEST/ca.pem"
chmod 644 "$CERT_DEST/client-cert.pem"
chmod 640 "$CERT_DEST/client-key.pem"

echo "Certificates installed with ownership: $RSYSLOG_USER:$RSYSLOG_GROUP"

# Install rsyslog config
echo "Installing rsyslog configuration..."
cp "$SCRIPT_DIR/rsyslog-tls.conf" "$CONF_DEST/99-forward-tls.conf"
chmod 644 "$CONF_DEST/99-forward-tls.conf"

# Validate and restart
echo "Validating rsyslog configuration..."
if rsyslogd -N1; then
    echo "Restarting rsyslog..."
    systemctl restart rsyslog
    echo ""
    echo "TLS forwarding configured successfully!"
    echo "Check status: systemctl status rsyslog"
    echo "Check logs:   journalctl -u rsyslog -f"
else
    echo "ERROR: rsyslog configuration validation failed!"
    echo "Please check the configuration manually."
    exit 1
fi
INSTALL_EOF
    chmod +x "$PKG_DIR/install.sh"

    # Generate one-time token
    TOKEN=$(openssl rand -hex 16)
    EXPIRY=$(($(date +%s) + TOKEN_VALIDITY))
    
    # Create tarball
    DOWNLOADS_DIR="${ROSI_BASE}/downloads/tls-packages"
    mkdir -p "$DOWNLOADS_DIR"
    chmod 755 "$DOWNLOADS_DIR"
    
    TARBALL="${DOWNLOADS_DIR}/${TOKEN}.tar.gz"
    tar -czf "$TARBALL" -C "$TEMP_DIR" "${CLIENT_NAME}-tls"
    chmod 644 "$TARBALL"
    
    # Create token metadata file
    cat > "${DOWNLOADS_DIR}/${TOKEN}.meta" << EOF
client=${CLIENT_NAME}
created=$(date -Iseconds)
expires=${EXPIRY}
EOF
    chmod 640 "${DOWNLOADS_DIR}/${TOKEN}.meta"
    
    # Clean up temp dir
    rm -rf "$TEMP_DIR"
    
    # Get download URL
    DOWNLOAD_URL="https://${TRAEFIK_DOMAIN:-localhost}/downloads/tls-packages/${TOKEN}.tar.gz"
    
    log_info ""
    log_info "Download package created!"
    log_info ""
    log_info "Download URL (one-time, expires in ${TOKEN_VALIDITY}s):"
    echo "  $DOWNLOAD_URL"
    log_info ""
    log_info "Or copy directly from server:"
    echo "  $TARBALL"
    log_info ""
    log_info "Client installation commands:"
    echo ""
    echo "curl -sLO '$DOWNLOAD_URL'"
    echo "tar xzf ${TOKEN}.tar.gz"
    echo "cd ${CLIENT_NAME}-tls && sudo ./install.sh"
    echo ""
    log_info "After installation, delete the download:"
    echo "rm -f ${TOKEN}.tar.gz"
fi
