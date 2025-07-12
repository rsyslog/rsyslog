rm -rf common
cp -r ../../../common common
docker build $1 -t rsyslog/rsyslog_dev_edge_ubuntu:16.04 .
rm -rf common
