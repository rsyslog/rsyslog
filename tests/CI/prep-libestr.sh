#!/bin/bash
echo "****** PREPARE libestr"
#set -o xtrace
PWD_HOME=$PWD
if [ ! -d "local_env" ]; then mkdir local_env; fi
if [ ! -d "local_env/install" ]; then mkdir local_env/install; fi
cd local_env
pwd
git clone git://github.com/rsyslog/libestr
cd libestr
autoreconf -fvi
./configure --prefix=/opt/rsyslog > /dev/null
#./configure --prefix=$PWD_HOME/local_env/install &> /dev/null
#find /opt/rsyslog
make
sudo make install
cd $PWD_HOME
