#!/bin/ash
if [ "$DEBUG" == "on" ]; then
	echo "container in debug mode, environment is: "
	env
	set -x
fi
if [ -f tools/$1 ]; then
	source tools/$1
else
	echo "ERROR: command not known: $*"
	echo
	source tools/help
	exit 1
fi
