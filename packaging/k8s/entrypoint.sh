#!/bin/bash
# entrypoint.sh for the Kubernetes-native rsyslog container.

# --- Functions ---

# Function to handle the SIGTERM signal.
# This is sent by Kubernetes when a pod is being terminated.
# We need to gracefully shut down rsyslog to avoid data loss.
graceful_shutdown() {
  echo "Caught SIGTERM, shutting down rsyslog gracefully..."
  # Kill the rsyslogd process, which will trigger the main_queue to save its state.
  kill $RSYSLOG_PID
  # Wait for rsyslogd to exit.
  wait $RSYSLOG_PID
  echo "rsyslogd has shut down."
  exit 0
}

# --- Main Script ---

# Register the signal handler for SIGTERM.
trap 'graceful_shutdown' SIGTERM

# Start rsyslog in the background.
# The CMD from the Dockerfile will be passed as arguments to this script.
"$@" &

# Capture the PID of the backgrounded rsyslogd process.
RSYSLOG_PID=$!

# Wait for the rsyslogd process to exit.
# This will keep the container running.
wait $RSYSLOG_PID
