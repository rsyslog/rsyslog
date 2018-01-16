#!/bin/bash
printf "running configure with\nCC:\t$CC\nCFLAGS:\t$CFLAGS\n"
autoreconf -fvi
./configure $RSYSLOG_CONFIGURE_OPTIONS $RSYSLOG_CONFIGURE_OPTIONS_EXTRA
