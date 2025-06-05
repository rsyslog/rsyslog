#!/bin/bash
set -e

# --- Common logic ---
if [ -n "$RSYSLOG_HOSTNAME" ]; then
    echo "Using pre-set container hostname"
else
    echo "Obtaining RSYSLOG_HOSTNAME from /etc/hostname"
    export RSYSLOG_HOSTNAME="$(cat /etc/hostname)"
fi
echo "rsyslog uses hostname '$RSYSLOG_HOSTNAME'"

# --- Role-specific logic ---
case "$RSYSLOG_ROLE" in
  docker)
    if [ -z "$REMOTE_SERVER_NAME" ]; then
        echo "ERROR: REMOTE_SERVER_NAME must be set in the environment." >&2
        exit 1
    fi

    if [ -z "$REMOTE_SERVER_PORT" ]; then
        export REMOTE_SERVER_PORT=514
        echo "REMOTE_SERVER_PORT not set, defaulting to 514"
    fi

    echo "Remote log target: ${REMOTE_SERVER_NAME}:${REMOTE_SERVER_PORT}"
    ;;
  collector)
    ;;
  minimal|*)
    ;;
esac

# --- Validate config before launch ---
if [ -z "$PERMIT_UNCLEAN_START" ]; then
    echo "Validating rsyslog config with rsyslogd -N1..."
    if ! rsyslogd -N1; then
        echo "ERROR: rsyslog configuration validation failed." >&2
        exit 1
    fi
else
    echo "PERMIT_UNCLEAN_START is set — skipping config validation."
fi

# --- Final Info ---
echo "rsyslog doc at https://www.rsyslog.com/, for enterprise support contact info@adiscon.com"

exec "$@"
