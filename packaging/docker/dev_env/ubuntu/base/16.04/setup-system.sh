# Whissi "special build" of 2017-12 autoconf-archive:
#wget --no-verbose http://build.rsyslog.com/CI/autoconf-archive_20170928-1adiscon1_all.deb
#dpkg -i autoconf-archive_20170928-1adiscon1_all.deb
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

