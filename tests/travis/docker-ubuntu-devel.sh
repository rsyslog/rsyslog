#!/bin/bash
set -e
TRAVIS_ROOT=`pwd`
docker run -v "$TRAVIS_ROOT":/home/travis rsyslog/rsyslog_dev_full:ubuntu_devel bash -c "cd /home/travis; ./tests/travis/run-ubuntu-devel.sh"
