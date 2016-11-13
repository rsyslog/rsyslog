#!/bin/bash

set -x

export loop_monitor=.loop_monitor
touch $loop_monitor
outfile=rsyslog.out.log

trap "rm $loop_monitor" SIGUSR1

function print_sigterm_receipt {
    echo 'received SIGTERM, ignoring it' >> $outfile
}
trap "print_sigterm_receipt" SIGTERM

while [ -e  $loop_monitor ]; do
    read log_line
    echo $log_line >> $outfile
done
