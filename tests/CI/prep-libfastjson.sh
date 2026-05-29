#!/bin/bash
echo "****** PREPARE libfastjson"
#set -o xtrace
PWD_HOME=$PWD
if [ ! -d "local_env" ]; then mkdir local_env; fi
if [ ! -d "local_env/install" ]; then mkdir local_env/install; fi
cd local_env || exit 1
pwd
git clone git://github.com/rsyslog/libfastjson
cd libfastjson || exit 1
autoreconf -fvi
./configure --prefix=/opt/rsyslog > /dev/null
make
sudo make install
cd "$PWD_HOME" || exit 1
#set +o xtrace
