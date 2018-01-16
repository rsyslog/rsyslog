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

printf "\n\n============ STEP: testing debugless build ================\n\n\n"
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA="--enable-debugless"
$DO_IN_CONTAINER make clean
$DO_IN_CONTAINER devtools/run-configure.sh
unset RSYSLOG_CONFIGURE_OPTIONS_EXTRA

# older style compile tests
printf "\n\n============ STEP: testing alpine build  ================\n\n\n"
$DO_IN_CONTAINER make clean
$RSYSLOG_HOME/tests/travis/docker-alpine.sh
