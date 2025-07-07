#!/bin/bash
# run compile-only tests under Travis
# This is specifically written to support Travis CI
set -e
DO_IN_CONTAINER="$RSYSLOG_HOME/devtools/devcontainer.sh"
printf "\n\n============ STEP: check code style ================\n\n\n"
$DO_IN_CONTAINER devtools/check-codestyle.sh

printf "\n\n============ STEP: run static analyzer ================\n\n\n"
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA="--disable-ksi-ls12"
$DO_IN_CONTAINER devtools/run-static-analyzer.sh
unset RSYSLOG_CONFIGURE_OPTIONS_EXTRA

$DO_IN_CONTAINER make clean
printf "\n\n============ STEP: testing debugless build ================\n\n\n"
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA="--enable-debugless"
export CC=gcc
export CFLAGS=
$DO_IN_CONTAINER devtools/run-configure.sh
$DO_IN_CONTAINER make check TESTS=""
unset RSYSLOG_CONFIGURE_OPTIONS_EXTRA

$DO_IN_CONTAINER make clean
printf "\n\n============ STEP: testing build without atomic ops ================\n\n\n"
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA="--disable-atomic-operations"
$DO_IN_CONTAINER devtools/run-configure.sh
$DO_IN_CONTAINER make check TESTS=""
unset RSYSLOG_CONFIGURE_OPTIONS_EXTRA

# #################### newer compilers ####################

$DO_IN_CONTAINER make clean
printf "\n\n============ STEP: gcc-9 compile test ================\n\n\n"
export CC=gcc-9
export CFLAGS=
$DO_IN_CONTAINER devtools/run-configure.sh
$DO_IN_CONTAINER make check TESTS=""

$DO_IN_CONTAINER make clean
printf "\n\n============ STEP: clang-9 compile test ================\n\n\n"
export CC=clang-9
export CFLAGS=
$DO_IN_CONTAINER devtools/run-configure.sh
$DO_IN_CONTAINER make check TESTS=""


# #################### older style compile tests####################
$DO_IN_CONTAINER make clean
printf "\n\n============ STEP: testing alpine build  ================\n\n\n"
$RSYSLOG_HOME/tests/travis/docker-alpine.sh
