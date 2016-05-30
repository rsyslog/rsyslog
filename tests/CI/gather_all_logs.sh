#!/bin/bash
# Some generic cleanup to be done before buildbot processes
# tests.
echo gather all logs left from make check.
echo you only need these if make check is terminated due to timeout
ls -tr tests/*.sh.log | xargs cat
