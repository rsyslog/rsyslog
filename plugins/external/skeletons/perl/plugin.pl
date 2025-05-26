#! /usr/bin/perl

#   A skeleton for a perl rsyslog output plugin
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
my $maxAtOnce = 1024;	# max nbr of messages that are processed within one batch
my $pollPeriod = 10;	#timeout
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
	my @msgs; 
	my $stdInLine; 
	my $msgsInBatch = 0; 
	while ($keepRunning) {
		#sleep(1);
		# We seem to have not timeout for select - or do we?
		if ($STDIN->can_read($pollPeriod)) {
			$stdInLine = <STDIN>;
			# Catch EOF, run onRecieve one last time and exit
			if (eof()){
				$keepRunning = 0;
				last;
			}
			if (length($stdInLine) > 0) {
				push (@msgs, $stdInLine); 

				$msgsInBatch++; 
				if ($msgsInBatch >= $maxAtOnce) {
					last;
				}
			}
		}
	}
	
	onReceive(@msgs);
}

onExit(); 
