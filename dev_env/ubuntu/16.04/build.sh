rm -rf common
cp -r ../../common common
docker build --rm -t rsyslog/rsyslog_dev_ubuntu:16.04 .
rm -rf common
