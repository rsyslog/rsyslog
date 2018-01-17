set -v
dnf -y update

echo STEP: essentials
# prevent systemd from stealing our core files (bad for testbench)
bash -c "echo core > /proc/sys/kernel/core_pattern"

# search for packages that contain <file>: dnf whatprovides <file>
dnf -y install sudo clang clang-analyzer git valgrind libtool autoconf automake flex bison \
	python-docutils python-sphinx python-devel \
	libuuid-devel libgcrypt-devel zlib-devel openssl-devel gnutls-devel \
	mysql-devel postgresql-devel libdbi-dbd-mysql libdbi-devel \
	net-snmp-devel systemd-devel hiredis-devel qpid-proton-c-devel redhat-rpm-config \
	libfaketime \
	lsof \
	curl libcurl-devel \
	libnet libnet-devel \
	mongo-c-driver-devel \
	czmq-devel \
	libmaxminddb-devel \
	gcc \
	gdb \
	nc \
	snappy-devel \
	cyrus-sasl-devel \
	cyrus-sasl-lib \
	autoconf-archive

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
#git clone https://github.com/guardtime/libksi.git
#cd libksi
#autoreconf -fvi
#./configure --prefix=/usr
#make -j2
#make install
#cd ..
#rm -r libksi

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

