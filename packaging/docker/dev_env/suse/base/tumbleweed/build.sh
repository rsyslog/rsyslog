set -e
docker build $1 --build-arg BUILD_TIME="$(date)" -t rsyslog/rsyslog_dev_base_suse:tumbleweed .
printf "\n\n================== BUILD DONE, NOW TESTING CONTAINER:"
docker run -ti --rm rsyslog/rsyslog_dev_base_suse:tumbleweed bash -c  "
set -e && \
git clone https://github.com/rsyslog/rsyslog.git && \
cd rsyslog && \
devtools/run-configure.sh
make -j4
"
if [ $? -eq 0 ]; then
	printf "\nREADY TO PUSH!\n"
	printf "\ndocker push rsyslog/rsyslog_dev_base_suse:tumbleweed\n"
fi
