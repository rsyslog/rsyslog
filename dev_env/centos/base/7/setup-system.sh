set -v
#yum -y update

echo STEP: essentials
# prevent systemd from stealing our core files (bad for testbench)
bash -c "echo core > /proc/sys/kernel/core_pattern"

# search for packages that contain <file>: yum whatprovides <file>

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

if [ "$LIBDIR_PATH" != "" ]; then
	export LIBDIR=--libdir=$LIBDIR_PATH
fi

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
(unset CFLAGS; ./configure --prefix=/usr $LIBDIR --CFLAGS="-g" ; make -j2)
make install
cd ..
# Note: we do NOT delete the source as we may need it to
# uninstall (in case the user wants to go back to system-default)

# finished installing helpers from source
cd ..

