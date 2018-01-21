#!/bin/ash
echo "WARNING: this is an experimental container - do not use in production"
echo
source internal/set-defaults
echo `cat CONTAINER.name` version `cat CONTAINER.release` - `cat CONTAINER.homepage`
echo `cat CONTAINER.copyright`
echo

if [ "$USE_VALGRIND" = "on" ]; then
	source tools/install-valgrind
	VALGRIND=valgrind
fi

if [ "$SYSLOG_APPLIANCE_DEBUG" = "on" ]; then
	echo APPLIANCE IS IN DEBUG MODE
	env
	ls -l /config
	#RSYSLOG_DEBUG_FLAG=-d
fi

if [ -f tools/$1 ]; then
	TO_RUN="$1"
	shift
	source tools/$TO_RUN
else
	echo "ERROR: command $1 not known"
	exit 1
fi
