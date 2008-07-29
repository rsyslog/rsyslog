# /bin/bash
# This is a simple shell script that carries out some checks against
# configurations we expect from some provided config files. We use
# rsyslogd's verifcation function. Note that modifications to the
# config elements, or even simple text changes, cause these checks to
# fail. However, it should be fairly easy to adapt them to the changed
# environment. And while nothing changed, they permit is to make sure
# that everything works well and is not broken by interim changes.
# Note that we always compare starting with the second output line.
# This is because the first line contains the rsyslog version ;)
# rgerhards, 2008-07-29
#
# Part of the testbench for rsyslog.
#
# Copyright 2008 Rainer Gerhards and Adiscon GmbH.
#
# This file is part of rsyslog.
#
# Rsyslog is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Rsyslog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
#
# A copy of the GPL can be found in the file "COPYING" in this distribution.
rm -f tmp
#
# check empty config file
#
../tools/rsyslogd -c3 -N1 -f/dev/null 2>&1 |tail --lines=+2 > tmp
cmp tmp DevNull.cfgtest
if [ ! $? -eq 0 ]; then
echo "DevNull.cfgtest failed"
exit 1
else
echo "DevNull.cfgtest succeeded"
fi;
#
# check missing config file
#
../tools/rsyslogd -c3 -N1 -f./This-does-not-exist 2>&1 |tail --lines=+2 > tmp
cmp tmp NoExistFile.cfgtest
if [ ! $? -eq 0 ]; then
echo "NoExistFile.cfgtest failed"
exit 1
else
echo "NoExistFile.cfgtest succeeded"
fi;
#
# check config with invalid directive
#
../tools/rsyslogd -c3 -u2 -N1 -f./cfg1.testin 2>&1 |tail --lines=+2 > tmp
cmp tmp cfg1.cfgtest
if [ ! $? -eq 0 ]; then
echo "cfg1.cfgtest failed"
exit 1
else
echo "cfg1.cfgtest succeeded"
fi;
#
# now check for included config file. We use a sample similar to
# the one with the invalid config directive, so that we may see
# an effect of the included config ;)
#
../tools/rsyslogd -c3 -u2 -N1 -f./cfg2.testin 2>&1 |tail --lines=+2 > tmp
cmp tmp cfg2.cfgtest
if [ ! $? -eq 0 ]; then
echo "cfg2.cfgtest failed"
exit 1
else
echo "cfg2.cfgtest succeeded"
fi;
#
# check included config file, where included file does not exist
#
../tools/rsyslogd -c3 -u2 -N1 -f./cfg3.testin 2>&1 |tail --lines=+2 > tmp
cmp tmp cfg3.cfgtest
if [ ! $? -eq 0 ]; then
echo "cfg3.cfgtest failed"
exit 1
else
echo "cfg3.cfgtest succeeded"
fi;
#
# check a reasonable complex, but correct, log file
#
../tools/rsyslogd -c3 -u2 -N1 -f./cfg4.testin 2>&1 |tail --lines=+2 > tmp
cmp tmp cfg4.cfgtest
if [ ! $? -eq 0 ]; then
echo "cfg4.cfgtest failed"
exit 1
else
echo "cfg4.cfgtest succeeded"
fi;
