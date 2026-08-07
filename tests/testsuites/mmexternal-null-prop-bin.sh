#!/bin/sh
# added 2026-08-07 by Julien Thomas, released under ASL 2.0
# mmexternal helper that replies, for every input message, with a single core
# property set to a bare JSON null. The property name comes from $1 (default
# "msg"). This exercises the NULL-value path of msgSetPropViaJSON(): a JSON
# null is stringified by jsonToString() to NULL, and the string setters
# (msg/rawmsg/syslogtag/hostname/...) then call strlen(NULL).
prop=${1:-msg}
while IFS= read -r line; do
	printf '{"%s": null}\n' "$prop"
done
