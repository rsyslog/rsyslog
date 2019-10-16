FROM	centos:7
RUN	yum -y update
RUN	yum -y install wget mock git sudo mc vim screen rsync createrepo rpm-sign
RUN	mkdir /private-files
VOLUME	/private-files
RUN	useradd pkg --uid 1001 -ms /bin/bash
RUN	usermod -aG wheel pkg
RUN	passwd -d pkg
WORKDIR	/home/pkg
ARG	CACHEBUST=1
RUN	git clone https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git
WORKDIR	/home/pkg/rsyslog-pkg-rhel-centos
RUN	sed -i "s/szLocalUser\=test/szLocalUser\=pkg/g" config.sh
RUN	mkdir /home/pkg/rsyslog-pkg-rhel-centos/yumrepo
VOLUME	/home/pkg/rsyslog-pkg-rhel-centos/yumrepo
COPY	extra/initenv.sh ./
COPY	extra/do_upload.sh ./
COPY	extra/sync_remote.sh ./
RUN	chmod +x ./initenv.sh
RUN	chmod +x ./do_upload.sh
RUN	chmod +x ./sync_remote.sh
COPY	extra/adisconextra.repo /etc/yum.repos.d/
RUN	yum -y update
RUN	yum -y install \
	autoconf \
	autoconf-archive \
	make \
	automake \
	flex \
	gcc \
	gcc-c++ \
	gdb \
	libtool \
	bison \
	python-docutils \
	libestr-devel \
	libfastjson4-devel \
	zlib-devel \
	libuuid-devel \
	libgcrypt-devel \
	libcurl-devel \
