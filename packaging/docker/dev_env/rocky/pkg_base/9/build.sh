#!/bin/bash
# Build and optionally tag the Rocky 9 daily RPM package-build base image.
# Image: rsyslog/rsyslog_dev_pkg_base_rocky:9
#
# Usage:
#   ./build.sh                 # normal build
#   ./build.sh --no-cache      # force full rebuild
# Extra docker build args may be passed as $1 (same as legacy pkg_base scripts).

set -e
docker build $1 -t rsyslog/rsyslog_dev_pkg_base_rocky:9 --build-arg CACHEBUST="$(date +%s)" .
printf "\n\n================== BUILD DONE\n"
printf "Image: rsyslog/rsyslog_dev_pkg_base_rocky:9\n"
printf "Publish with: docker push rsyslog/rsyslog_dev_pkg_base_rocky:9\n"
