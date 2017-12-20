#!/bin/bash
mkdir _tmp_stylecheck
cd _tmp_stylecheck
git clone https://github.com/rsyslog/codestyle
cd codestyle
gcc --std=c99 stylecheck.c -o stylecheck
cd ../..
find -name "*.[ch]" | xargs _tmp_stylecheck/codestyle/stylecheck -w -f -l 125 && \
	_tmp_stylecheck/codestyle/stylecheck plugins/imfile/imfile.c -l 125
# Note: we do stricter checks for some code sources that have been
# sufficiently cleaned up. That after the "&&" part of the statement.
rm -rf codestyle
