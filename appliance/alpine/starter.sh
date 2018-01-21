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

if [ -f tools/$1 ]; then
	source tools/$1
else
	echo "ERROR: command not known: $*"
	echo
	source tools/help
	exit 1
fi
