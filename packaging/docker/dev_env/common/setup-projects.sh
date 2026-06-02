#!/bin/sh
#export SUDO=
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
#export LD_LIBRARY_PATH=/usr/local/lib

# end config settings
CI_HOME=$(pwd)
export CI_HOME
echo CI_HOME: $CI_HOME
mkdir $CI_HOME/proj
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/libestr.git
cd libestr || exit 1
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make -j2 || exit $?
$SUDO make -j2 install || exit $?

printf "\n\n================== libfastjson =======================\n\n"
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/libfastjson.git
cd libfastjson || exit 1
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --disable-journal && make -j2 || exit $?
$SUDO make -j2 install || exit $?


printf "\n\n================== liblogging =======================\n\n"
cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/liblogging.git
cd liblogging || exit 1
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


### main project ###
printf "\n\n================== rsyslog =======================\n\n"
printf "This is primarily built to make sure that the helper\n"
printf "projects are correctly built and installed\n\n"

cd "$CI_HOME/proj" || exit 1
git clone https://github.com/rsyslog/rsyslog.git
cd rsyslog || exit 1
git pull
mkdir utils
echo "./configure --prefix=/usr/local $RSYSLOG_CONFIGURE_OPTIONS" --enable-compile-warnings=yes > utils/conf
chmod +x utils/conf

autoreconf -fvi && utils/conf && make -j2 || exit $?
