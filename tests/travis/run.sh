# this script runs the travis CI testbench. It's easier and more
# powerful this way than using .travis.yml (plus recommended by travis support)
#
set -o xtrace # we want to see the execution steps
set -e  # abort on first failure

if [ "x$BUILD_FROM_TARBALL" == "xYES" ]; then autoreconf -fvi && ./configure && make dist && mv *.tar.gz rsyslog.tar.gz && mkdir unpack && cd unpack && tar xzf ../rsyslog.tar.gz && ls -ld rsyslog* && cd rsyslog* ; fi
pwd
autoreconf --force --verbose --install
if [ "x$GROK" == "xYES" ]; then export GROK="--enable-mmgrok"; fi
# at this point, the environment should be setup for ./configure
if [ "$CC" == "clang" ] && [ "$DISTRIB_CODENAME" == "trusty" ]; then export CC="clang-3.6"; fi
$CC -v
env
export CONFIG_FLAGS="--prefix=/opt/rsyslog --build=x86_64-pc-linux-gnu --host=x86_64-pc-linux-gnu --mandir=/usr/share/man --infodir=/usr/share/info --datadir=/usr/share --sysconfdir=/etc --localstatedir=/var/lib --disable-dependency-tracking --enable-silent-rules --libdir=/usr/lib64 --docdir=/usr/share/doc/rsyslog --disable-generate-man-pages --enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp --disable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-mysql --enable-mysql-tests --enable-usertools=no --enable-gt-ksi --enable-libdbi --enable-pgsql --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-omamqp1 $JOURNAL_OPT $HIREDIS_OPT $ENABLE_KAFKA $NO_VALGRIND $GROK"
./configure  $CONFIG_FLAGS
export USE_AUTO_DEBUG="off" # set to "on" to enable this for travis
make
if [ "x$CHECK" == "xYES" ] ; then make check ; fi
if [ -f tests/test-suite.log ] ; then cat tests/test-suite.log; fi
if [ "x$CHECK" == "xYES" ] ; then make distcheck ; fi
if [ "x$STAT_AN" == "xYES" ] ; then make clean; CFLAGS="-O2 -std=c99"; ./configure $CONFIG_FLAGS ; fi
if [ "x$STAT_AN" == "xYES" ] ; then cd compat; $SCAN_BUILD --status-bugs make && cd .. ; fi
# we now build those components that we know to need some more work
# they will not be included in the later static analyzer run. But by
# explicitely listing the modules which do not work, we automatically
# get new modules/files covered.
if [ "x$STAT_AN" == "xYES" ] ; then cd runtime; make lmnet_la-net.lo libgcry_la-libgcry.lo ; cd .. ;  fi
if [ "x$STAT_AN" == "xYES" ] ; then $SCAN_BUILD --status-bugs make ; fi
