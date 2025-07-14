cp -r ../../../common/ common
docker build $1 -t rsyslog/rsyslog_dev_base_fedora:27 .
rm -r common
