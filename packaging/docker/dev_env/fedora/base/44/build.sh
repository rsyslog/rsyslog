#!/bin/sh
set -e
docker build "$@" -t rsyslog/rsyslog_dev_base_fedora:44 .
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
DOCKER_TTY_FLAGS=""
if [ -t 0 ] && [ -t 1 ]; then
	DOCKER_TTY_FLAGS="-ti"
fi

docker run $DOCKER_TTY_FLAGS rsyslog/rsyslog_dev_base_fedora:44 bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure \$RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
# shellcheck disable=SC2181
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_fedora:44\n"
fi
