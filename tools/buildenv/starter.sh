#!/bin/ash
if [ -f tools/$1 ]; then
	source tools/$1
else
	echo "ERROR: command not known: $*"
	echo
	source tools/help
	exit 1
fi
