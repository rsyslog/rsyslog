docker build $1 -t rsyslog/rsyslog_dev_base_ubuntu-arm:18.04 -f Dockerfile.arm .
printf "ready to run docker push rsyslog/rsyslog_dev_base_ubuntu-arm:18.04\n"
