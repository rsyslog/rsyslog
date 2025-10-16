OPEN_EULER_VERSION=24.03-lts
set -e
docker build $1 -t rsyslog/rsyslog_dev_base_openeuler:$OPEN_EULER_VERSION . --progress=plain
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:\n"
docker run -ti -u $(id -u):$(id -g) rsyslog/rsyslog_dev_base_openeuler:$OPEN_EULER_VERSION bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
autoreconf -fi && \
./configure \$RSYSLOG_CONFIGURE_OPTIONS --enable-compile-warnings=yes  && \
make -j4
"
if [ $? -eq 0 ]; then
        printf "\nREADY TO PUSH!\n"
        printf "\ndocker push rsyslog/rsyslog_dev_base_openeuler:$OPEN_EULER_VERSION\n"
fi
rm -rf ./rsyslog
