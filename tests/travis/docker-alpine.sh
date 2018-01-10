set -e
TRAVIS_ROOT=`pwd`
docker run -v "$TRAVIS_ROOT":/home/travis rsyslog/rsyslog_dev_full:alpine_latest bash -c "ls -l /home/travis"
docker run -v "$TRAVIS_ROOT":/home/travis rsyslog/rsyslog_dev_full:alpine_latest bash -c "cd /home/travis; ./tests/travis/run-alpine.sh"
