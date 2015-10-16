#!/bin/bash
# Copyright 2015-01-29 by Tim Eifler
# This file is part of the rsyslog project, released  under ASL 2.0
# The configuration test should pass because of the good config file.
echo ===============================================================================
echo \[abort-uncleancfg-goodcfg-check.sh\]: testing abort on unclean configuration
echo "testing a good Configuration verification run"
. $srcdir/diag.sh init
../tools/rsyslogd  -C -N1 -f$srcdir/testsuites/abort-uncleancfg-goodcfg.conf -M../runtime/.libs:../.libs
if [ $? -ne 0 ]; then
   echo "Error: config check fail"
   exit 1 
fi
. $srcdir/diag.sh exit

