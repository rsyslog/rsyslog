set -e
cd $RSYSLOG_HOME/devtools
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA="--disable-ksi-ls12"
./devcontainer.sh devtools/run-static-analyzer.sh
