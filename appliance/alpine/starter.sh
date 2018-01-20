#!/bin/ash
echo "WARNING: this is an experimental container - do not use in production"
echo

if [ ! -f /config/container_config ]; then
	cat tools/no-config.errmsg
	exit 1
else
	source /config/container_config
fi;

echo `cat CONTAINER.name` version `cat CONTAINER.release` - `cat CONTAINER.homepage`
echo `cat CONTAINER.copyright`
echo

if [ "$USE_VALGRIND" = "on" ]; then
	source tools/install-valgrind
	VALGRIND=valgrind
fi

if [ "$SYSLOG_APPLIANCE_DEBUG" = "on" ]; then
	env
	ls -l /config
	#RSYSLOG_DEBUG_FLAG=-d
fi
exec $VALGRIND /usr/sbin/rsyslogd -n $RSYSLOG_DEBUG_FLAG -f/etc/rsyslog.conf
