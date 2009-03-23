# rsyslog parser tests
package require Expect
package require udp 1.0

set rsyslogdPID [spawn "../tools/rsyslogd" "-c4" "-ftestruns/parser.conf" "-u2" "-n" "-iwork/rsyslog.pid" "-M../runtime/.libs"];
#interact;
#puts "pid: $rsyslogdPID";
#sleep 1;
#expect "\n";
expect "}}"; # eat startup message
set udpSock [udp_open];
udp_conf $udpSock 127.0.0.1 514
set files [glob *.parse1]
puts "done init\n";


foreach testcase $files {
	puts "File $testcase";
	set fp [open "$testcase" r];
	fconfigure $fp -buffering line
	#set data [read $fp];
	#set data [split $data "\n"];
	gets $fp input
	puts "Line 1: $input\n";

	puts $udpSock $input;
	flush $udpSock;


	set i 1
	expect -re "{{.*}}";
	set result $expect_out(buffer);

	#puts "MSG $i: '$expect_out(buffer)'";
	puts "MSG $i: '$result'\n";
	set i [expr {$i + 1}];
	
}

exec kill $rsyslogdPID;
close $udpSock;
