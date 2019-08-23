printf "\n\n================== ENTER DOCKER CONTAINER\n"
docker run --privileged --cap-add=SYS_ADMIN -ti rsyslog/rsyslog_dev_pkg_centos:7
