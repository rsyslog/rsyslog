set -e
docker build $1 -t rsyslog/rsyslog_dev_base_debian:13 .
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
tty_flags=""
if [ -t 0 ] && [ -t 1 ]; then
	tty_flags="-ti"
fi
docker run --rm $tty_flags rsyslog/rsyslog_dev_base_debian:13 bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure $RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_debian:13\n"
fi
