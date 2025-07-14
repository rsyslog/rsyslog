rm -rf common
cp -r ../common common
docker build $1 --rm -t rsyslog/rsyslog_dev_full:alpine_latest .
rm -rf common
