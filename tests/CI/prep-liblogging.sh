#!/bin/bash
echo "****** PREPARE liblogging"
#set -o xtrace
PWD_HOME=$PWD
if [ ! -d "local_env" ]; then mkdir local_env; fi
if [ ! -d "local_env/install" ]; then mkdir local_env/install; fi
cd local_env
pwd
git clone git://github.com/rsyslog/liblogging
cd liblogging
autoreconf -fvi
./configure --prefix=/opt/rsyslog --disable-man-pages > /dev/null
make
sudo make install
cd $PWD_HOME
#set +o xtrace
