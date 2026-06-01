#!/bin/bash

printf '<= OK\n' >> "$RSYSLOG_OMPROG_RESTART_OUT"
printf 'OK\n'

while IFS= read -r line; do
	printf '=> %s\n' "$line" >> "$RSYSLOG_OMPROG_RESTART_OUT"
	if [ -e "$RSYSLOG_OMPROG_RESTART_OK" ]; then
		printf '<= OK\n' >> "$RSYSLOG_OMPROG_RESTART_OUT"
		printf 'OK\n'
	else
		printf '<= ERROR\n' >> "$RSYSLOG_OMPROG_RESTART_OUT"
		printf 'ERROR\n'
	fi
done
