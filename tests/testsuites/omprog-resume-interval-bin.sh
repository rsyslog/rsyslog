#!/bin/bash

outfile="$RSYSLOG_OUT_LOG"
statefile=${RSYSLOG_DYNNAME:-$outfile}.state
attempt=0

if [[ -f "$statefile" ]]; then
	read -r attempt < "$statefile"
fi

status="OK"
echo "<= $status" >> "$outfile"
echo "$status"

in_transaction=false
transaction_payload=
while IFS= read -r line; do
	message=${line//$'\n'}

	if [[ "$message" == "BEGIN TRANSACTION" ]]; then
		in_transaction=true
		transaction_payload=
		status="OK"
	elif [[ "$message" == "COMMIT TRANSACTION" ]]; then
		in_transaction=false
		if [[ $attempt -lt 2 ]]; then
			status="Error: transient commit failure"
		else
			status="OK"
		fi
		printf 'commit %s %s %s %s\n' \
			"$attempt" "$(date +%s)" "${status// /_}" "$transaction_payload" >> "$outfile"
		((attempt++))
		echo "$attempt" > "$statefile"
	else
		if [[ $in_transaction == true ]]; then
			transaction_payload=$message
			status="DEFER_COMMIT"
		else
			status="Error: received a message out of a transaction"
		fi
	fi

	echo "=> $message" >> "$outfile"
	echo "<= $status" >> "$outfile"
	echo "$status"
done

exit 0
