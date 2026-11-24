#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Added 2017-12-16 by Stephen Workman, released under ASL 2.0
# Verify accepted timestamp formats, full-string rejection of trailing
# message text while preserving one final ASCII space, invalid input errors,
# and that an empty string or sign alone is not treated as UNIX epoch zero.

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omstdout/.libs/omstdout")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# $DebugLevel 2

set $!result!date_auto_1 = is_time("Oct  5 01:10:11");
set $!result!errno_date_auto_1 = script_error();
set $!result!date_auto_2 = is_time("2017-10-05T01:10:11Z");
set $!result!errno_date_auto_2 = script_error();
set $!result!date_auto_suffix = is_time("2017-10-05T01:10:11Z trailing-text");
set $!result!errno_date_auto_suffix = script_error();
set $!result!date_auto_space = is_time("2017-10-05T01:10:11Z ");
set $!result!errno_date_auto_space = script_error();
set $!result!date_auto_3164_suffix = is_time("Oct  5 01:10:11 trailing-text");
set $!result!errno_date_auto_3164_suffix = script_error();
set $!result!date_auto_3164_space = is_time("Oct  5 01:10:11 ");
set $!result!errno_date_auto_3164_space = script_error();
set $!result!date_auto_3 = is_time("2017-10-05T01:10:11-03:00");
set $!result!errno_date_auto_3 = script_error();
set $!result!date_auto_4 = is_time("90210");
set $!result!errno_date_auto_4 = script_error();

set $!result!date_explicit_1 = is_time("Oct  5 01:10:11", "date-rfc3164");
set $!result!errno_date_explicit_1 = script_error();
set $!result!date_explicit_3164_suffix = is_time("Oct  5 01:10:11 trailing-text", "date-rfc3164");
set $!result!errno_date_explicit_3164_suffix = script_error();
set $!result!date_explicit_3164_space = is_time("Oct  5 01:10:11 ", "date-rfc3164");
set $!result!errno_date_explicit_3164_space = script_error();
set $!result!date_explicit_2 = is_time("2017-10-05T01:10:11Z", "date-rfc3339");
set $!result!errno_date_explicit_2 = script_error();
set $!result!date_explicit_suffix = is_time("2017-10-05T01:10:11Z trailing-text", "date-rfc3339");
set $!result!errno_date_explicit_suffix = script_error();
set $!result!date_explicit_space = is_time("2017-10-05T01:10:11Z ", "date-rfc3339");
set $!result!errno_date_explicit_space = script_error();
set $!result!date_explicit_3 = is_time("2017-10-05T01:10:11+04:00", "date-rfc3339");
set $!result!errno_date_explicit_3 = script_error();
set $!result!date_explicit_4 = is_time(90210, "date-unix");
set $!result!errno_date_explicit_4 = script_error();
set $!result!date_explicit_5 = is_time(-88, "date-unix");
set $!result!errno_date_explicit_5 = script_error();
set $!result!date_explicit_6 = is_time(0, "date-unix");
set $!result!errno_date_explicit_6 = script_error();
set $!result!date_explicit_7 = is_time("90210", "date-unix");
set $!result!errno_date_explicit_7 = script_error();
set $!result!date_explicit_8 = is_time("-88", "date-unix");
set $!result!errno_date_explicit_8 = script_error();
set $!result!date_explicit_zero_string = is_time("0", "date-unix");
set $!result!errno_date_explicit_zero_string = script_error();

# Bad dates
set $!result!date_fail_1 = is_time("Oct 88 01:10:11");
set $!result!errno_date_fail_1 = script_error();
set $!result!date_fail_2 = is_time("not at all a date");
set $!result!errno_date_fail_2 = script_error();

# Wrong format
set $!result!date_fail_3 = is_time("Oct  5 01:10:11", "date-rfc3339");
set $!result!errno_date_fail_3 = script_error();
set $!result!date_fail_4 = is_time("2017-10-05T01:10:11Z", "date-rfc3164");
set $!result!errno_date_fail_4 = script_error();
set $!result!date_fail_5 = is_time("Oct  5 01:10:11", "date-unix");
set $!result!errno_date_fail_5 = script_error();

# Invalid format
set $!result!date_fail_6 = is_time("90210", "date-spoonix");
set $!result!errno_date_fail_6 = script_error();
set $!result!date_fail_empty_auto = is_time("");
set $!result!errno_date_fail_empty_auto = script_error();
set $!result!date_fail_empty_unix = is_time("", "date-unix");
set $!result!errno_date_fail_empty_unix = script_error();
set $!result!date_fail_sign_auto = is_time("-");
set $!result!errno_date_fail_sign_auto = script_error();
set $!result!date_fail_sign_unix = is_time("-", "date-unix");
set $!result!errno_date_fail_sign_unix = script_error();
set $!result!date_after_empty = is_time(0, "date-unix");
set $!result!errno_date_after_empty = script_error();

template(name="outfmt" type="string" string="%!result%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
local4.* :omstdout:;outfmt
'

startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown

# Our fixed and calculated expected results
export EXPECTED='{ "date_auto_1": 1, "errno_date_auto_1": 0, "date_auto_2": 1, "errno_date_auto_2": 0, "date_auto_suffix": 0, "errno_date_auto_suffix": 1, "date_auto_space": 1, "errno_date_auto_space": 0, "date_auto_3164_suffix": 0, "errno_date_auto_3164_suffix": 1, "date_auto_3164_space": 1, "errno_date_auto_3164_space": 0, "date_auto_3": 1, "errno_date_auto_3": 0, "date_auto_4": 1, "errno_date_auto_4": 0, "date_explicit_1": 1, "errno_date_explicit_1": 0, "date_explicit_3164_suffix": 0, "errno_date_explicit_3164_suffix": 1, "date_explicit_3164_space": 1, "errno_date_explicit_3164_space": 0, "date_explicit_2": 1, "errno_date_explicit_2": 0, "date_explicit_suffix": 0, "errno_date_explicit_suffix": 1, "date_explicit_space": 1, "errno_date_explicit_space": 0, "date_explicit_3": 1, "errno_date_explicit_3": 0, "date_explicit_4": 1, "errno_date_explicit_4": 0, "date_explicit_5": 1, "errno_date_explicit_5": 0, "date_explicit_6": 1, "errno_date_explicit_6": 0, "date_explicit_7": 1, "errno_date_explicit_7": 0, "date_explicit_8": 1, "errno_date_explicit_8": 0, "date_explicit_zero_string": 1, "errno_date_explicit_zero_string": 0, "date_fail_1": 0, "errno_date_fail_1": 1, "date_fail_2": 0, "errno_date_fail_2": 1, "date_fail_3": 0, "errno_date_fail_3": 1, "date_fail_4": 0, "errno_date_fail_4": 1, "date_fail_5": 0, "errno_date_fail_5": 1, "date_fail_6": 0, "errno_date_fail_6": 1, "date_fail_empty_auto": 0, "errno_date_fail_empty_auto": 1, "date_fail_empty_unix": 0, "errno_date_fail_empty_unix": 1, "date_fail_sign_auto": 0, "errno_date_fail_sign_auto": 1, "date_fail_sign_unix": 0, "errno_date_fail_sign_unix": 1, "date_after_empty": 1, "errno_date_after_empty": 0 }'

# FreeBSD's cmp does not support reading from STDIN
cmp <(echo "$EXPECTED") $RSYSLOG_OUT_LOG

if [[ $? -ne 0 ]]; then
  printf "Invalid function output detected!\n"
  printf "Expected: $EXPECTED\n"
  printf "Got:      "
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
