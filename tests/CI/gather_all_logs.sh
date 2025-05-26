#!/bin/bash
# Some generic cleanup to be done before buildbot processes
# tests.
echo gather all logs left from make check.
echo you only need these if make check is terminated due to timeout
# IMPORTANT: ! -name "rstb_*" excludes $RSYSLOGDYNNAME*.log
( cd tests ;
find . -maxdepth 1 -print0 -name "*.log" ! -name "rstb_*" -exec cat {} +; )
