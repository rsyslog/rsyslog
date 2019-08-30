FROM	centos:7
RUN	yum -y update
RUN	yum -y install wget mock git sudo mc vim screen rsync createrepo rpm-sign
RUN	mkdir /private-files
VOLUME	/private-files
RUN	useradd pkg --uid 1001 -ms /bin/bash
RUN	usermod -aG wheel pkg
RUN	passwd -d pkg
WORKDIR	/home/pkg
RUN	git clone https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git
WORKDIR	/home/pkg/rsyslog-pkg-rhel-centos
RUN	sed -i "s/szLocalUser\=test/szLocalUser\=pkg/g" config.sh
COPY	extra/sync_remote.sh .
COPY	extra/do_upload.sh .
COPY	extra/passfile.txt ./
COPY	extra/.ssh /home/pkg/.ssh/
COPY	extra/.gnupg /home/pkg/.gnupg/
COPY	extra/.ssh /root/.ssh/
COPY	extra/.gnupg /root/.gnupg/
USER	root
RUN	chmod +x sync_remote.sh
RUN	chmod +x do_upload.sh
RUN	chmod -R 600 /home/pkg/.ssh/
RUN	chmod -R 600 /home/pkg/.gnupg/
RUN	chmod -R 600 /root/.ssh/
RUN	chmod -R 600 /root/.gnupg/
RUN	mkdir /home/pkg/rsyslog-pkg-rhel-centos/yumrepo
# First rpmrepo SYNC is cached
RUN	./sync_remote.sh
# Second rpmrepo SYNC is forced always!
ARG	CACHEBUST=1
RUN	./sync_remote.sh
# Git update and copy of mock files is forced always
ARG	CACHEBUST=1
RUN	git pull && yes | cp -rf etc-mock/* /etc/mock/
ARG	CACHEBUST=1
RUN	chown -R pkg ./ 
USER	pkg
# Get rsyslog source for testing
RUN	wget https://www.rsyslog.com/files/download/rsyslog/rsyslog-8.1908.0.tar.gz
RUN	mv rsyslog-8.1908.0.tar.gz rpmbuild/SOURCES/
