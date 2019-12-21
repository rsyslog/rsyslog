#!/bin/bash
# this perform a travis cron job
#set -x

if [ "$TRAVIS_EVENT_TYPE" != "cron" ]; then
	echo cron job not executed under non-cron run
	exit
fi

source tests/travis/install.sh
source /etc/lsb-release

# download coverity tool
mkdir coverity
cd coverity
wget --no-verbose http://build.rsyslog.com/CI/cov-analysis.tar.gz
if [ $? -ne 0 ]; then
	echo Download Coverity analysis tool failed!
	exit 1
fi
tar xzf cov*.tar.gz
rm -f cov*.tar.gz
export PATH="coverity/$(ls -d cov*)/bin:$PATH"
cd ..
# Coverity scan tool installed

# we need Guardtime libksi here, otherwise we cannot check the KSI component
git clone https://github.com/guardtime/libksi.git
cd libksi
autoreconf -fvi
./configure --prefix=/usr
make -j
sudo make install
cd ..

# prep rsyslog for submission
autoreconf -vfi
# explicit ./configure as this needs to be consistent across all builds
# for Coverity
./configure -enable-testbench --enable-imdiag --enable-imfile --enable-impstats --enable-imptcp --enable-mmanon --enable-mmaudit --enable-mmfields --enable-mmjsonparse --enable-mmpstrucdata --enable-mmsequence --enable-mmutf8fix --enable-mail --enable-omprog --enable-omruleset --enable-omstdout --enable-omuxsock --enable-pmaixforwardedfrom --enable-pmciscoios --enable-pmcisconames --enable-pmlastmsg --enable-pmsnare --enable-libgcrypt --enable-mmnormalize --disable-omudpspoof --enable-relp=yes --enable-snmp --disable-mmsnmptrapd --enable-gnutls --enable-mysql=yes --enable-usertools=yes --enable-ksi-ls12 --enable-libdbi --enable-pgsql --enable-omhttpfs --enable-elasticsearch --enable-valgrind --enable-ommongodb --enable-omamqp1=no --enable-imjournal --enable-omjournal --enable-compile-warnings=error --enable-testbench --enable-compile-warnings=yes --without-valgrind-testbench --enable-omrelp-default-port=13515 --disable-liblogging-stdlog --enable-mmrm1stspace -enable-omkafka --enable-imkafka --enable-mmdblookup --enable-mmcount --enable-ommongodb --enable-openssl --enable-omhttp --enable-imdocker
make clean
cov-build --dir cov-int make -j4
tar czf rsyslog.tgz cov-int
ls -l rsyslog.tgz
# we make this FAIL to not thrash our allowance if things go wrong!
curl --form token=$COVERITY_TOKEN \
  --form email=rgerhards@adiscon.com \
  --form file=@rsyslog.tgz \
  --form version="master branch head" \
  --form description="$(git log -1|head -1)" \
  https://scan.coverity.com/builds?project=rsyslog%2Frsyslog
CURL_RESULT=$?
echo curl returned $CURL_RESULT
if [ $CURL_RESULT -ne 0 ]; then
	echo Upload to Coverity failed, curl returned $CURL_RESULT
	exit 1
fi
