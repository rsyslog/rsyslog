#!/bin/bash
printf "running configure with\nCC:\t$CC\nCFLAGS:\t$CFLAGS\n"
if [ "$RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE" != "" ]; then
	RSYSLOG_CONFIGURE_OPTIONS="$RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE"
fi

CONFIGURE_OPTS_OVERRIDE=
if [ -n "$CI_CONFIGURE_CACHE" ]
then
	if [ -z "$CI_CONFIGURE_CACHE_FILE" ]
	then
		CI_CONFIGURE_CACHE_FILE=config.cache
	fi
	CONFIGURE_CACHE_OPTS="--cache-file=$CI_CONFIGURE_CACHE_FILE"
else
	CONFIGURE_CACHE_OPTS=
fi
grep "sanitize.*=.*address" <<< "$CFLAGS" >/dev/null
if [ $? -eq 0 ]; then
	printf "\n==========================================================\n"
	printf "WARNING: address sanitizer requested. libfaketime is not\n"
	printf "compatible with it. DISABLED LIBFAKETIME TESTS.\n"
	printf "==========================================================\n\n"
	export CONFIGURE_OPTS_OVERRIDE=--disable-libfaketime
fi
autoreconf -fvi
./configure $CONFIGURE_CACHE_OPTS $RSYSLOG_CONFIGURE_OPTIONS $RSYSLOG_CONFIGURE_OPTIONS_EXTRA $CONFIGURE_OPTS_OVERRIDE
