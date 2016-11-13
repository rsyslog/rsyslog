#!/bin/bash

export loop_monitor=.loop_monitor
touch $loop_monitor
outfile=rsyslog.out.log

trap "rm $loop_monitor" SIGUSR1

function print_sigterm_receipt {
    echo 'received SIGTERM' >> $outfile
}
trap "print_sigterm_receipt" SIGTERM

while [ -e  $loop_monitor ]; do
    read log_line
    if [ "x$log_line" == "x" ]; then
	log_line='Exit due to read-failure'
	rm $loop_monitor
    fi
    echo $log_line >> $outfile
done

sleep .2 # so we can catch a TERM and log it, in case it comes (negative test for signalOnClose=off)

echo "PROCESS TERMINATED (last msg: $log_line)" >> $outfile

exit 0
