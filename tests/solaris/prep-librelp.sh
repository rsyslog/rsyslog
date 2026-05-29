#!/bin/bash
set -o xtrace
PWD_HOME=$PWD
mkdir local_env
mkdir local_env/install
cd local_env || exit 1
pwd
git clone http://github.com/rsyslog/librelp
cd librelp || exit 1
git log -2
env | grep FLAGS
autoreconf -fvi
./configure --prefix="$PWD_HOME/local_env/install"
gmake -j2 V=1
gmake install
pwd
ls ../install
