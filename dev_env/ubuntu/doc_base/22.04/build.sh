#!/bin/bash
set -e
# Use --no-cache to rebuild image
docker build $1 -t rsyslog/rsyslog_dev_doc_base_ubuntu:22.04 .
printf "\n\n================== BUILD DONE\n"
