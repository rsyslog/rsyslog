#!/bin/bash
set -e
docker run -v "$RSYSLOG_HOME":/home/travis rsyslog/rsyslog_dev_full:alpine_latest bash -c "cd /home/travis; ./tests/travis/run-alpine.sh"
