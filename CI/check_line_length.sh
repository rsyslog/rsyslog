#!/bin/bash
mkdir _tmp_stylecheck
cd _tmp_stylecheck
git clone https://github.com/rsyslog/codestyle
cd codestyle
gcc --std=c99 stylecheck.c -o stylecheck
cd ../..
find -name "*.[ch]" | xargs _tmp_stylecheck/codestyle/stylecheck -w -f -l 120 && \
	_tmp_stylecheck/codestyle/stylecheck plugins/imfile/imfile.c
rm -rf _tmp_stylecheck

# and now we also check the code style
# for discssion on style, see also
# https://github.com/rsyslog/rsyslog/pull/1479
find . -name "*.[ch]" -exec clang-format -i {} +
git diff --exit-code
if [ $? -ne 0 ]; then
	echo ====================================================================
	echo ERROR: CODE STYLE VIOLATION
	echo ====================================================================
	echo The rsyslog code style has been defined by a community consensus
	echo process and we require that all contributions abide to this style.
	echo The style is described in clang-format control file format inside
	echo file .clang-format in the project home directory.
	echo If you just want to automatically reformat your contribution, you
	echo can simply run
	echo \$ find . -name "*.[ch]" -exec clang-format -i {} +
	echo in the project home directory. This should take care of everything.
	echo ====================================================================
	exit 1
fi
