#!/bin/sh
#export SUDO=
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
#export LD_LIBRARY_PATH=/usr/local/lib

# end config settings
CI_HOME=$(pwd)
export CI_HOME
echo CI_HOME: $CI_HOME

printf "\n\n================== libfastjson =======================\n\n"
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/libfastjson.git
cd libfastjson || exit 1
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --disable-journal && make -j2 || exit $?
$SUDO make -j2 install || exit $?


printf "\n\n================== liblognorm =======================\n\n"
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/liblognorm.git
cd liblognorm || exit 1
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make -j2 || exit $?
$SUDO make -j2 install || exit $?

printf "\n\n================== liblrelp =======================\n\n"
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/librelp.git
cd librelp || exit 1
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --enable-compile-warnings=yes && make -j2 || exit $?
$SUDO make -j2 install || exit $?
