#export SUDO=
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
#export LD_LIBRARY_PATH=/usr/local/lib

# end config settings
export CI_HOME=`pwd`
echo CI_HOME: $CI_HOME
mkdir $CI_HOME/proj
cd $CI_HOME/proj
git clone https://github.com/rsyslog/libestr.git
cd libestr
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make || exit $?
$SUDO make install || exit $?

cd $CI_HOME/proj
git clone https://github.com/rsyslog/libfastjson.git
cd libfastjson
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --disable-journal && make || exit $?
$SUDO make install || exit $?


cd $CI_HOME/proj
git clone https://github.com/rsyslog/liblogging.git
cd liblogging
git pull
autoreconf -fvi && ./configure --prefix=/usr/local --disable-journal && make || exit $?
$SUDO make install || exit $?

cd $CI_HOME/proj
git clone https://github.com/rsyslog/liblognorm.git
cd liblognorm
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make || exit $?
$SUDO make install || exit $?

cd $CI_HOME/proj
git clone https://github.com/rsyslog/librelp.git
cd librelp
git pull
autoreconf -fvi && ./configure --prefix=/usr/local && make || exit $?
$SUDO make install || exit $?


### main project ###
# This is primarily built to make sure that the helper
# projects are correctly built and installed

cd $CI_HOME/proj
git clone https://github.com/rsyslog/rsyslog.git
cd rsyslog
git pull
mkdir utils
echo "./configure --prefix=/usr/local -enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-mysql --enable-mysql-tests --enable-usertools=no --enable-libdbi --enable-pgsql --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb=no --enable-omamqp1 --enable-imjournal --enable-omjournal" > utils/conf
chmod +x utils/conf

autoreconf -fvi && utils/conf && make || exit $?
