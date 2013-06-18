# check if the configuration test run detects invalid config files.
#
# Part of the testbench for rsyslog.
#
# Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#set -x
echo \[validation-run.sh\]: testing configuraton validation
echo "testing a failed configuration verification run"
../tools/rsyslogd  -dn -u2 -c4 -N1 -f$srcdir/testsuites/invalid.conf -M../runtime/.libs:../.libs
if [ $? -ne 1 ]; then
   echo "after test 1: return code ne 1"
   exit 1
fi
echo testing a valid config verification run
../tools/rsyslogd -u2 -c4 -N1 -f$srcdir/testsuites/valid.conf -M../runtime/.libs:../.libs
if [ $? -ne 0 ]; then
   echo "after test 2: return code ne 0"
   exit 1
fi
echo testing empty config file
../tools/rsyslogd -u2 -c4 -N1 -f/dev/null -M../runtime/.libs:../.libs
if [ $? -ne 1 ]; then
   echo "after test 3: return code ne 1"
   exit 1
fi
echo SUCCESS: validation run tests
