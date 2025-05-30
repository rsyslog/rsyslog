#!/bin/bash
# this checks the rsyslog codestyle. It expects that
# rsyslog_stylecheck
# is already installed inside the system
# if in doubt, run it on one of the development containers
doc="https://www.rsyslog.com/doc/master/development/dev_codestyle.html"

find -name "*.[ch]" | xargs rsyslog_stylecheck -l 120
if [ $? -ne 0 ]; then
	printf '\n\n====================================================================\n'
	printf 'Codestyle Error:\n'
	printf 'Your code is not compliant with the rsyslog codestyle policies\n'
	printf 'See here for more information: %s\n' "$doc"
	exit 1
else
	printf 'Codestyle check was successful!\n'
fi
