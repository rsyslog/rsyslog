#!/bin/ash
source internal/set-defaults
source /config/container_config

if [ "$CONTAINER_SILENT" != "on" ]; then
	echo `cat CONTAINER.name` version `cat CONTAINER.release` - `cat CONTAINER.homepage`
	echo `cat CONTAINER.copyright`
	echo
	echo "WARNING: this is an experimental container - do not use in production"
	echo
fi

if [ "$USE_VALGRIND" = "on" ]; then
	source internal/install-valgrind
	export VALGRIND=valgrind
fi

if [ "$SYSLOG_APPLIANCE_DEBUG" = "on" ]; then
	echo APPLIANCE IS IN DEBUG MODE
	env
	ls -l /config
	#RSYSLOG_DEBUG_FLAG=-d
fi

# Set $RSYSLOG_CONFIG_BASE64 to overwrite /etc/rsyslog.conf with base64 encoded config
# Useful to inject rsyslog.conf file without mounting it into the container
if [ ! -z "$RSYSLOG_CONFIG_BASE64" ]; then
        echo "Writing RSYSLOG_CONFIG_BASE64"
        echo "$RSYSLOG_CONFIG_BASE64" | base64 -d > /config/rsyslog.conf
        export RSYSLOG_CONF="/config/rsyslog.conf"
fi

# If RSYSLOG_CONF is empty, we want to default to /etc/rsyslog.conf
# This allows the user to overwrite what configuration file is being used
if [ -z "$RSYSLOG_CONF" ]; then
        export RSYSLOG_CONF="/etc/rsyslog.conf"
fi

echo "Using rsyslog configuration file: $RSYSLOG_CONF"


if [ -f tools/$1 ]; then
	source tools/$1
else
	echo "ERROR: command not known: $*"
	echo
	source tools/help
	exit 1
fi
