#!/bin/bash
set -o xtrace
PWD_HOME=$PWD
mkdir local_env
mkdir local_env/install
cd local_env || exit 1
pwd
git clone http://github.com/rsyslog/libfastjson
cd libfastjson || exit 1
git log -2
autoreconf -fvi
./configure --prefix="$PWD_HOME/local_env/install"
gmake -j2
gmake install
pwd
ls ../install
