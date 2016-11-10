#!/bin/bash

export loop_monitor=.loop_monitor
touch $loop_monitor
outfile=rsyslog.out.log

trap "rm $loop_monitor" SIGUSR1

function print_sigterm_receipt {
    echo 'received SIGTERM' >> $outfile
    exit
}
trap "print_sigterm_receipt" SIGTERM

while [ -e  $loop_monitor ]; do
    read log_line
    if [ "x$log_line" != "x" ]; then
	echo $log_line >> $outfile
    fi
done

echo "PROCESS TERMINATED" >> $outfile
