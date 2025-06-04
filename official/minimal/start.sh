#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status

# Check if HOST_HOSTNAME is set and not empty
if [ -n "$RSYSLOG_HOSTNAME" ]; then
    echo "Using pre-set container hostname"
else
    echo "Obtaining RSYSLOG_HOSTNAME from /etc/hostname"
    export RSYSLOG_HOSTNAME="$(cat /etc/hostname)"
fi
echo "rsyslog uses hostname '$RSYSLOG_HOSTNAME'"
echo "rsyslog version: $(rsyslogd -v|head -n1| grep -o '^.*)')"
echo "rsyslog doc at https://www.rsyslog.com/"

# Execute the main command of your container (e.g., your application)
# This is crucial! Without this, your container will just exit after setting the hostname.
# Replace 'your_actual_app_command' with whatever your container is supposed to run.
# For example, if it's a web server, it might be 'nginx -g "daemon off;"' or 'apache2ctl -D FOREGROUND'
# If it's a simple script, it might be 'node index.js' or 'python app.py'
exec "$@" # This executes the command passed to the container (CMD in Dockerfile, or command in docker-compose.yml)
