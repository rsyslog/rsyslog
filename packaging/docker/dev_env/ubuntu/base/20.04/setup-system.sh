#!/bin/sh
export DEBIAN_FRONTEND=noninteractive
set -v

#echo STEP: essentials
#apt-get install -y 

# CURRENTLY NOT NEEDED - 20.04 provides the required gcc versions by default!
#echo STEP: set gcc-7 repo
# currently the best repo for gcc-7 we can find...
#add-apt-repository ppa:ubuntu-toolchain-r/test -y

# ALSO NOT NEEDED, 20.04 has a sufficiently current clang by default!
# LLVM repository
#echo STEP: set clang 5 repo
#echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-5.0 main" > /etc/apt/sources.list.d/llvm.list
#wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key| apt-key add -

# 0mq repository
#echo STEP: set 0mq repo
#echo "deb http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/ ./" > /etc/apt/sources.list.d/0mq.list
#wget -O - http://download.opensuse.org/repositories/network:/messaging:/zeromq:/git-draft/xUbuntu_16.04/Release.key | apt-key add -

# Ready to go -- Now done in Dockerfile!!!
# echo STEP: install main components
#apt-get install -y \

# Note: we need Java for some dynamic tests (e.g. ElasticSearch)

# Whissi "special build" of 2017-12 autoconf-archive:
#wget --no-verbose http://build.rsyslog.com/CI/autoconf-archive_20170928-1adiscon1_all.deb
#dpkg -i autoconf-archive_20170928-1adiscon1_all.deb
# BAD work-around
#apt-get install -y autoconf-archive
#rm autoconf-archive_20170928-1adiscon1_all.deb

# Now build some custom libraries for whom there are not packages
mkdir helper-projects
(
cd helper-projects || exit 1

# code style checker - not yet packaged
git clone https://github.com/rsyslog/codestyle
(
	cd codestyle || exit 1
	gcc --std=c99 stylecheck.c -o stylecheck
	mv stylecheck /usr/bin/rsyslog_stylecheck
)
rm -r codestyle

# we need Guardtime libksi here, otherwise we cannot check the KSI component
git clone https://github.com/guardtime/libksi.git
(
	cd libksi || exit 1
	autoreconf -fvi
	./configure --prefix=/usr
	make -j2
	make install
)
rm -r libksi

# NOTE: we do NOT install coverity, because
# - it is pretty large
# - seldomly used
# - quickly installed when required (no build process)

# we need the latest librdkafka as there as always required updates
git clone https://github.com/edenhill/librdkafka
(
	cd librdkafka || exit 1
	(unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j2)
	make install
)
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# finished installing helpers from source
)
