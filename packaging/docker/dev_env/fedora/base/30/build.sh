set -e
docker build $1 -t rsyslog/rsyslog_dev_base_fedora:30 .
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
docker run -ti rsyslog/rsyslog_dev_base_fedora:30 bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure \$RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_fedora:30\n"
fi
