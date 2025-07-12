#export SUDO=
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
#export LD_LIBRARY_PATH=/usr/local/lib

# end config settings
export CI_HOME=`pwd`
echo CI_HOME: $CI_HOME

printf "\n\n================== libfastjson =======================\n\n"
cd $CI_HOME/proj
git clone https://github.com/rsyslog/libfastjson.git
cd libfastjson
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --disable-journal && make -j2 || exit $?
$SUDO make -j2 install || exit $?


printf "\n\n================== liblognorm =======================\n\n"
cd $CI_HOME/proj
git clone https://github.com/rsyslog/liblognorm.git
cd liblognorm
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make -j2 || exit $?
$SUDO make -j2 install || exit $?

printf "\n\n================== liblrelp =======================\n\n"
cd $CI_HOME/proj
git clone https://github.com/rsyslog/librelp.git
cd librelp
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --enable-compile-warnings=yes && make -j2 || exit $?
$SUDO make -j2 install || exit $?
