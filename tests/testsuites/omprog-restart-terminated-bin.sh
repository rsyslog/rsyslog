#!/bin/bash

outfile=$RSYSLOG_OUT_LOG
terminate=false

function handle_sigusr1 {
    echo "Received SIGUSR1, will terminate after the next message" >> $outfile
    terminate=true
}
trap "handle_sigusr1" SIGUSR1

function handle_sigterm {
    echo "Received SIGTERM, terminating" >> $outfile
    exit 1
}
trap "handle_sigterm" SIGTERM

echo "Starting" >> $outfile

# Tell rsyslog we are ready to start processing messages
echo "OK"

read log_line
while [[ -n "$log_line" ]]; do
    echo "Received $log_line" >> $outfile

    if [[ $terminate == true ]]; then
        # Terminate prematurely by closing pipe, without confirming the message
        echo "Terminating without confirming the last message" >> $outfile
        exit 1
    fi

    # Tell rsyslog we are ready to process the next message
    echo "OK"

    read log_line
done

echo "Terminating normally" >> $outfile
exit 0
