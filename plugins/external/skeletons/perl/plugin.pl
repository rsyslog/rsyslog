#! /usr/bin/perl

#	A skeleton for a perk rsyslog output plugin
#   Copyright (C) 2014 by Adiscon GmbH
#
#   This file is part of rsyslog.
#  
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#   
#         http://www.apache.org/licenses/LICENSE-2.0
#         -or-
#         see COPYING.ASL20 in the source distribution
#   
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

use strict;
use warnings;
use IO::Handle;
use IO::Select;

# skeleton config parameters
my $pollPeriod = 0.75;	# the number of seconds between polling for new messages
my $maxAtOnce = 1024;	# max nbr of messages that are processed within one batch

# App logic global variables
my $OUTFILE;			# Output Filehandle


sub onInit {
	#	Do everything that is needed to initialize processing (e.g.
	#	open files, create handles, connect to systems...)

	open $OUTFILE, ">>/tmp/logfile" or die $!;
	$OUTFILE->autoflush(1);
}

sub onReceive {
	#	This is the entry point where actual work needs to be done. It receives
	#	a list with all messages pulled from rsyslog. The list is of variable
	#	length, but contains all messages that are currently available. It is
	#	suggest NOT to use any further buffering, as we do not know when the
	#	next message will arrive. It may be in a nanosecond from now, but it
	#	may also be in three hours...
	foreach(@_) {
		print $OUTFILE $_;
	}
}

sub onExit {
	#	Do everything that is needed to finish processing (e.g.
	#	close files, handles, disconnect from systems...). This is
	#	being called immediately before exiting.
	close($OUTFILE);
}

#-------------------------------------------------------
#This is plumbing that DOES NOT need to be CHANGED
#-------------------------------------------------------
onInit(); 

# Read from STDIN
$STDIN = IO::Select->new();
$STDIN->add(\*STDIN);

# Enter main Loop
my $keepRunning = 1; 
while ($keepRunning) {
	print "Looping! \n";
	my @msgs; 
	my $stdInLine; 
	my $msgsInBatch = 0; 
	while ($keepRunning) {
		#sleep(1);
		if ($STDIN->can_read($pollPeriod)) {
			$stdInLine = <STDIN>;
			print "Got '$stdInLine' from STDIN\n";
			if (length($stdInLine) > 0) {
				push (@msgs, $stdInLine); 
				print "Added to msgs: " . $stdInLine; 

				$msgsInBatch++; 
				if ($msgsInBatch >= $maxAtOnce) {
					last;
				}
			}
		}
	}
	
	print "Flushing msgs...\n"; 
	onReceive(@msgs);
}

onExit(); 
