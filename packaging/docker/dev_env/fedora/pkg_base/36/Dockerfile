FROM	fedora:36
# Workarround for a bug affecting buildkit with yum when ulimit is unlimited
# https://github.com/docker/buildx/issues/379#issuecomment-1196517905
RUN echo "* soft nofile 1024000" >> /etc/security/limits.conf && \
    echo "* hard nofile 1024000" >> /etc/security/limits.conf && \
    echo "session required pam_limits.so" >> /etc/pam.d/common-session && \
    echo "ulimit -n 1024000" >> /etc/profile.d/ulimit.sh
# Install packages
RUN	dnf -y update
RUN	dnf -y install wget mock git sudo mc vim screen rsync createrepo rpm-sign yum crypto-policies-scripts.noarch
RUN	update-crypto-policies --set LEGACY
RUN	mkdir /private-files
VOLUME	/private-files
RUN	useradd pkg --uid 1001 -ms /bin/bash
RUN	usermod -aG wheel pkg
RUN	passwd -d pkg
WORKDIR	/home/pkg
COPY	extra/.rpmmacros ./
ARG	CACHEBUST=1
RUN	git config --global --add safe.directory /home/pkg/rsyslog-pkg-rhel-centos
RUN	git clone https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git
WORKDIR	/home/pkg/rsyslog-pkg-rhel-centos
RUN	git remote add alorbach https://github.com/alorbach/rsyslog-pkg-rhel-centos.git
RUN	mkdir /home/pkg/rsyslog-pkg-rhel-centos/yumrepo
VOLUME	/home/pkg/rsyslog-pkg-rhel-centos/yumrepo
COPY	extra/initenv.sh ./
COPY	extra/do_upload.sh ./
COPY	extra/do_upload_delete.sh ./
COPY	extra/sync_remote.sh ./
COPY	extra/sync_remote_delete.sh ./
COPY	extra/cleanup_repo.sh ./
COPY	extra/resignrepo.sh ./
RUN	chmod +x ./initenv.sh
RUN	chmod +x ./do_upload.sh
RUN	chmod +x ./do_upload_delete.sh
RUN	chmod +x ./sync_remote.sh
RUN	chmod +x ./sync_remote_delete.sh
RUN	chmod +x ./cleanup_repo.sh
#COPY	extra/adisconextra.repo /etc/yum.repos.d/
RUN	dnf -y update
RUN	dnf -y install \
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
	libfastjson-devel \
	zlib-devel \
	libuuid-devel \
	libgcrypt-devel \
	libcurl-devel \
	subscription-manager
