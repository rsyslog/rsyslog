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
# call: tclsh parser.tcl /director/with/testcases
# 
# Copyright (C) 2009 by Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.

package require Expect
package require udp 1.0
log_user 0; # comment this out if you would like to see rsyslog output for testing

if {$argc > 1} {
	puts "invalid number of parameters, usage: tclsh parser.tcl /directory/with/testcases";
	exit 1;
}
if {$argc == 0 } {
	set srcdir ".";
} else {
	set srcdir "$argv";
}

set rsyslogdPID [spawn "../tools/rsyslogd" "-c4" "-f$srcdir/testruns/parser.conf" "-u2" "-n" "-irsyslog.pid" "-M../runtime/.libs"];
#interact;
expect "}}"; # eat startup message
set udpSock [udp_open];
udp_conf $udpSock 127.0.0.1 12514
set files [glob "$srcdir/testruns/*.parse1"]
set failed 0;
puts "\nExecuting parser test suite...";

set i 0;

foreach testcase $files {
	puts "testing $testcase";
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

	set result $expect_out(buffer);
	set result [string trimleft $result "\{\{"];
	set result [string trimright $result "\}\}"];

	if { $result != $expected } {
		puts "failed!";
		puts "expected: '$expected'\n";
		puts "returned: '$result'\n";
		puts "\n";
		set failed [expr {$failed + 1}];
	};
	set i [expr {$i + 1}];
}

exec kill $rsyslogdPID;
close $udpSock;

puts "Total number of tests: $i, number of failed tests: $failed.\n";
if { $failed != 0 } { exit 1 };
