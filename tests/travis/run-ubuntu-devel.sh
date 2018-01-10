set -e
export CFLAGS="-g -Wall -Wextra -O2"
autoreconf -fvi
./configure --prefix=/usr/local --enable-testbench $RSYSLOG_CONFIGURE_OPTIONS
make -j2 check TESTS=""
exit
set +e
make check
RETCODE=$?
echo RETCODE $RETCODE
cat tests/test-suite.log
set -e
exit $RETCODE
