set -v

echo STEP: essentials
# prevent systemd from stealing our core files (bad for testbench)
#bash -c "echo core > /proc/sys/kernel/core_pattern" # does not work inside container

# search for packages that contain <file>: yum whatprovides <file>

# Now build some custom libraries for whom there are not packages
mkdir helper-projects
cd helper-projects

if [ "$LIBDIR_PATH" != "" ]; then
	export LIBDIR=--libdir=$LIBDIR_PATH
fi

#set -e
# libgrok - not packaged, manual build also does not work ATM
#git clone https://github.com/jordansissel/grok
#cd grok
##autoreconf -fvi
##./configure --libdir=/usr/lib64
#make
#make install
#cd ..
#rm -r grok

# we need Guardtime libksi here, otherwise we cannot check the KSI component
git clone https://github.com/guardtime/libksi.git
cd libksi
autoreconf -fvi
./configure --libdir=/usr/lib64
make -j2
make install
cd ..
rm -r libksi

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

