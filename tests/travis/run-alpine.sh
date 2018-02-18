set -e
autoreconf -fvi
./configure --prefix=/usr/local $RSYSLOG_CONFIGURE_OPTIONS
make -j2 check TESTS=""
