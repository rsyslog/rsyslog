#!/bin/bash
echo "****** PREPARE libfastjson"
#set -o xtrace
PWD_HOME=$PWD
if [ ! -d "local_env" ]; then mkdir local_env; fi
if [ ! -d "local_env/install" ]; then mkdir local_env/install; fi
cd local_env
pwd
git clone git://github.com/rsyslog/libfastjson
cd libfastjson
autoreconf -fvi
./configure --prefix=/opt/rsyslog > /dev/null
make
sudo make install
cd $PWD_HOME
#set +o xtrace
