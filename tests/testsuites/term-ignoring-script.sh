#!/bin/bash

set -x

export loop_monitor=.loop_monitor
touch $loop_monitor

trap "rm $loop_monitor" SIGUSR1
trap "echo TERM signal received, ignoring" SIGTERM

while [ -e  $loop_monitor ]; do
    read log_line
    echo $log_line >> rsyslog.out.log
done
