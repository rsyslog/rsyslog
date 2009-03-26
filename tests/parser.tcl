# rsyslog parser tests
# This is a first version, and can be extended and improved for
# sure. But it is far better than nothing. Please note that this
# script works together with the config file AND easily extensible
# test case files (*.parse1) to run a number of checks. All test
# cases are executed, even if there is a failure early in the
# process. When finished, the numberof failed tests will be given.
#
# Note: a lot of things are not elegant, but at least they work...
# Even simple things seem to be somewhat non-simple if you are
# not sufficiently involved with tcl/expect ;) -- rgerhards
# 
# Copyright (C) 2009 by Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.



# HELP   HELP   HELP   HELP   HELP   HELP   HELP   HELP
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# If you happen to know how to disable rsyslog's
# stdout from appearing on the "real" stdout, please
# let me know. This is annouying, but I have no more
# time left to invest finding a solution (as the
# rest basically works well...).
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

package require Expect
package require udp 1.0

set rsyslogdPID [spawn "../tools/rsyslogd" "-c4" "-ftestruns/parser.conf" "-u2" "-n" "-iwork/rsyslog.pid" "-M../runtime/.libs"];
#interact;
expect "}}"; # eat startup message
set udpSock [udp_open];
udp_conf $udpSock 127.0.0.1 514
set files [glob "testruns/*.parse1"]
set failed 0;
puts "\n";

set i 1;

foreach testcase $files {
	puts "testing $testcase ...";
	set fp [open "$testcase" r];
	fconfigure $fp -buffering line
	gets $fp input
	gets $fp expected
	# assemble "expected" to match the template we use
	close $fp;

	# send to daemon
	puts $udpSock $input;
	flush $udpSock;

	# get response and compare
	expect -re "{{.*}}";
	puts "\n"; # at least we make the output readbale...

	set result $expect_out(buffer);
	set result [string trimleft $result "\{\{"];
	set result [string trimright $result "\}\}"];

	if { $result != $expected } {
		puts "test $i failed!\n";
		puts "expected: '$expected'\n";
		puts "returned: '$result'\n";
		puts "\n";
		set failed [expr {$failed + 1}];
	};
	set i [expr {$i + 1}];
}

exec kill $rsyslogdPID;
close $udpSock;

puts "Number of failed test: $failed.\n";
