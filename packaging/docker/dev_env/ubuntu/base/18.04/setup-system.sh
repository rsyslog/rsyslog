export DEBIAN_FRONTEND=noninteractive
set -v

#echo STEP: essentials
#apt-get install -y 

# CURRENTLY NOT NEEDED - 18.04 provides very current gcc by default!
#echo STEP: set gcc-7 repo
# currently the best repo for gcc-7 we can find...
#add-apt-repository ppa:ubuntu-toolchain-r/test -y

# ALSO NOT NEEDED, 18.04 has 6.0 by default!
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
cd helper-projects

# code style checker - not yet packaged
git clone https://github.com/rsyslog/codestyle
cd codestyle
gcc --std=c99 stylecheck.c -o stylecheck
mv stylecheck /usr/bin/rsyslog_stylecheck
cd ..
rm -r codestyle

# we need Guardtime libksi here, otherwise we cannot check the KSI component
git clone https://github.com/guardtime/libksi.git
cd libksi
autoreconf -fvi
./configure --prefix=/usr
make -j2
make install
cd ..
rm -r libksi

# NOTE: we do NOT install coverity, because
# - it is pretty large
# - seldom used
# - quickly installed when required (no build process)

# we need the latest librdkafka as there as always required updates
git clone https://github.com/edenhill/librdkafka
cd librdkafka
(unset CFLAGS; ./configure --prefix=/usr --CFLAGS="-g" ; make -j2)
make install
cd ..
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# finished installing helpers from source
cd ..

