set -e
docker build $1 -t rsyslog/rsyslog_dev_base_debian:10 .
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
docker run --rm -ti rsyslog/rsyslog_dev_base_debian:10 bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure \$RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_debian:10\n"
fi
