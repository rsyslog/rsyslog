#!/bin/bash
printf "running configure with\nCC:\t$CC\nCFLAGS:\t$CFLAGS\n"
export CONFIGURE_OPTS_OVERRIDE=
grep "sanitize.*=.*address" <<< "$CFLAGS" >/dev/null
if [ $? -eq 0 ]; then
	printf "\n==========================================================\n"
	printf "WARNING: address sanitizer requested. libfaketime is not\n"
	printf "compatible with it. DISABLED LIBFAKETIME TESTS.\n"
	printf "==========================================================\n\n"
	export CONFIGURE_OPTS_OVERRIDE=--disable-libfaketime
fi
autoreconf -fvi
./configure $RSYSLOG_CONFIGURE_OPTIONS $RSYSLOG_CONFIGURE_OPTIONS_EXTRA $CONFIGURE_OPTS_OVERRIDE
