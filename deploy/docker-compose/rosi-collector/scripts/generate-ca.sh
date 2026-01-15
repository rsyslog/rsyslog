#!/usr/bin/env bash
## @file generate-ca.sh
## @brief Generate CA and server certificates for rsyslog TLS
## @description
##   Creates a local Certificate Authority and server certificate for
##   rsyslog TLS encryption. Called automatically by init.sh when
##   SYSLOG_TLS_ENABLED=true and certificates don't exist.
##
## @usage
##   ./generate-ca.sh [OPTIONS]
##
## @option -d, --dir DIR       Certificate directory (default: ../certs)
## @option -h, --hostname NAME Server hostname for certificate
## @option --ca-days DAYS      CA validity in days (default: 3650)
## @option --server-days DAYS  Server cert validity (default: 365)
## @option -f, --force         Regenerate even if exists
## @option --help              Show this help

set -euo pipefail

# Defaults
CERT_DIR=""
HOSTNAME=""
CA_DAYS=3650
SERVER_DAYS=365
FORCE=false
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

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Generate CA and server certificates for rsyslog TLS.

Options:
  -d, --dir DIR       Certificate directory (default: /srv/central/certs)
  -h, --hostname NAME Server hostname for certificate (REQUIRED)
  --ca-days DAYS      CA certificate validity in days (default: 3650 = 10 years)
  --server-days DAYS  Server certificate validity in days (default: 365 = 1 year)
  -f, --force         Regenerate certificates even if they exist
  --help              Show this help message

Environment:
  ROSI_HOME           Override base directory (default: /srv/central)

Examples:
  $(basename "$0") -h logs.example.com
  $(basename "$0") -h logs.example.com -d /opt/rosi-collector/certs
  $(basename "$0") -h logs.example.com --ca-days 7300 --server-days 730

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
        -h|--hostname)
            HOSTNAME="$2"
            shift 2
            ;;
        --ca-days)
            CA_DAYS="$2"
            shift 2
            ;;
        --server-days)
            SERVER_DAYS="$2"
            shift 2
            ;;
        -f|--force)
            FORCE=true
            shift
            ;;
        --help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Set default cert dir if not specified
if [[ -z "$CERT_DIR" ]]; then
    CERT_DIR="${ROSI_BASE}/certs"
fi

# Hostname is required
if [[ -z "$HOSTNAME" ]]; then
    log_error "Hostname is required. Use -h or --hostname"
    exit 1
fi

# Create certificate directory
mkdir -p "$CERT_DIR"
CERT_DIR="$(cd "$CERT_DIR" && pwd)"  # Get absolute path

log_info "Certificate directory: $CERT_DIR"
log_info "Server hostname: $HOSTNAME"

# Check if CA already exists
if [[ -f "$CERT_DIR/ca.pem" && -f "$CERT_DIR/ca-key.pem" && "$FORCE" != "true" ]]; then
    log_info "CA certificate already exists. Use --force to regenerate."
    
    # Check if server cert exists
    if [[ -f "$CERT_DIR/server-cert.pem" && -f "$CERT_DIR/server-key.pem" ]]; then
        log_info "Server certificate already exists."
        exit 0
    fi
fi

# Generate CA if needed or forced
if [[ ! -f "$CERT_DIR/ca.pem" || "$FORCE" == "true" ]]; then
    log_info "Generating CA certificate (valid for $CA_DAYS days)..."
    
    # Generate CA private key
    openssl genrsa -out "$CERT_DIR/ca-key.pem" 4096 2>/dev/null
    
    # Generate CA certificate
    openssl req -new -x509 \
        -days "$CA_DAYS" \
        -key "$CERT_DIR/ca-key.pem" \
        -out "$CERT_DIR/ca.pem" \
        -subj "/CN=ROSI Collector CA/O=rsyslog/OU=TLS" \
        2>/dev/null
    
    # Secure CA key
    chmod 600 "$CERT_DIR/ca-key.pem"
    chmod 644 "$CERT_DIR/ca.pem"
    
    log_info "CA certificate created: $CERT_DIR/ca.pem"
fi

# Generate server certificate
log_info "Generating server certificate for: $HOSTNAME (valid for $SERVER_DAYS days)..."

# Create OpenSSL config for SAN (Subject Alternative Names)
# This is required for modern TLS clients
cat > "$CERT_DIR/server.cnf" << EOF
[req]
default_bits = 2048
prompt = no
default_md = sha256
distinguished_name = dn
req_extensions = req_ext

[dn]
CN = $HOSTNAME
O = rsyslog
OU = TLS Server

[req_ext]
subjectAltName = @alt_names

[alt_names]
DNS.1 = $HOSTNAME
DNS.2 = localhost
IP.1 = 127.0.0.1
EOF

# Generate server private key
openssl genrsa -out "$CERT_DIR/server-key.pem" 2048 2>/dev/null

# Generate server CSR
openssl req -new \
    -key "$CERT_DIR/server-key.pem" \
    -out "$CERT_DIR/server.csr" \
    -config "$CERT_DIR/server.cnf" \
    2>/dev/null

# Sign server certificate with CA
openssl x509 -req \
    -days "$SERVER_DAYS" \
    -in "$CERT_DIR/server.csr" \
    -CA "$CERT_DIR/ca.pem" \
    -CAkey "$CERT_DIR/ca-key.pem" \
    -CAcreateserial \
    -out "$CERT_DIR/server-cert.pem" \
    -extensions req_ext \
    -extfile "$CERT_DIR/server.cnf" \
    2>/dev/null

# Set permissions
chmod 600 "$CERT_DIR/server-key.pem"
chmod 644 "$CERT_DIR/server-cert.pem"

# Clean up temporary files
rm -f "$CERT_DIR/server.csr" "$CERT_DIR/server.cnf"

log_info "Server certificate created: $CERT_DIR/server-cert.pem"

# Display certificate info
log_info ""
log_info "Certificate Summary:"
log_info "===================="
log_info "CA Certificate:     $CERT_DIR/ca.pem"
log_info "CA Private Key:     $CERT_DIR/ca-key.pem (KEEP SECURE!)"
log_info "Server Certificate: $CERT_DIR/server-cert.pem"
log_info "Server Private Key: $CERT_DIR/server-key.pem"
log_info ""
log_info "CA Expiry:     $(openssl x509 -in "$CERT_DIR/ca.pem" -noout -enddate | cut -d= -f2)"
log_info "Server Expiry: $(openssl x509 -in "$CERT_DIR/server-cert.pem" -noout -enddate | cut -d= -f2)"
log_info ""
log_info "Next steps:"
log_info "  1. Distribute ca.pem to all clients"
log_info "  2. Generate client certificates with: generate-client-cert.sh"
log_info "  3. Restart rsyslog container to load certificates"
