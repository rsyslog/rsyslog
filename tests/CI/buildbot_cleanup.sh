#!/bin/bash
# This script gathers and sends to stdout all log files created during
# make check. This is useful if make check is terminated due to timeout,
# in which case autotools unfortunately does not provide us a way to
# gather error information.
rm -f tests/*.sh.log
