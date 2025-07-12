UBUNTU_VERSION=24.04
set -e
docker build $1 -t rsyslog/rsyslog_dev_base_ubuntu:$UBUNTU_VERSION . --progress=plain
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
docker run -ti -u $(id -u):$(id -g) rsyslog/rsyslog_dev_base_ubuntu:$UBUNTU_VERSION bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure \$RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
rm -rf ./rsyslog
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_ubuntu:$UBUNTU_VERSION\n"
fi
