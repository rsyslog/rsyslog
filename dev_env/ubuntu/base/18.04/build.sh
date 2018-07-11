mkdir local_dep_cache
cp ../../../common/local_dep_cache/* local_dep_cache
docker build $1 -t rsyslog/rsyslog_dev_base_ubuntu:18.04 .
rm -r local_dep_cache
