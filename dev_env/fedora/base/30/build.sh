#cp -r ../../../common/ common
docker build $1 -t rsyslog/rsyslog_dev_base_fedora:30 .
#rm -r common
