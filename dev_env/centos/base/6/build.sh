cp -r ../../../common/ common
docker build $1 -t rsyslog/rsyslog_dev_base_centos:6 .
rm -r common
