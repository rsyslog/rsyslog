printf "use ./build.sh --no-cache to disable cache\n"
docker build $1 -t rsyslog/syslog_appliance_ubuntu:16.04 .
