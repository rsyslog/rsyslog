#!/bin/bash
# Regression test for https://github.com/rsyslog/rsyslog/issues/4486.
# With net.enableDNS="off", config processing must not call getaddrinfo() to
# canonicalize a non-FQDN local host name.  The preload helpers return a
# non-FQDN from gethostname() and write a marker if that synthetic name is ever
# passed to getaddrinfo(); a clean -N1 run with no marker proves DNS was avoided.
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"

export RSYSLOG_PRELOAD=".libs/liboverride_gethostname_nonfqdn.so:.libs/liboverride_getaddrinfo.so"
export RSYSLOG_GETADDRINFO_MARKER="$RSYSLOG_DYNNAME.getaddrinfo.marker"

generate_conf
add_conf '
global(net.enableDNS="off")
'
if ! rsyslogd_config_check; then
	printf 'rsyslogd -N1 failed unexpectedly\n'
	error_exit 1
fi
if [ -e "$RSYSLOG_GETADDRINFO_MARKER" ]; then
	printf 'getaddrinfo() was called for the synthetic local hostname:\n'
	cat "$RSYSLOG_GETADDRINFO_MARKER"
	error_exit 1
fi

exit_test
