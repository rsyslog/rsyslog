rm -rf common
cp -r ../../common common
sudo docker build --rm -t rsyslog/rsyslog_dev_full:ubuntu_devel .
rm -rf common
