#!/bin/sh
# Helper for mmexternal responseTimeout tests.
# It consumes one message and then stops responding until rsyslog kills it.
while IFS= read -r _line; do
	sleep 60
done
