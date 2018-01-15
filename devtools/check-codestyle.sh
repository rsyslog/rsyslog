#!/bin/bash
# this checks the rsyslog codestyle. It expects that
# rsyslog_stylecheck
# is already installed inside the system
# if in doubt, run it on one of the development containers
set -e
find -name "*.[ch]" | xargs rsyslog_stylecheck -w -f -l 120
# Note: we do stricter checks for some code sources that have been
# sufficiently cleaned up.
rsyslog_stylecheck plugins/imfile/imfile.c -l 120
