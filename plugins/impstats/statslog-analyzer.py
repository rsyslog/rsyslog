#! /usr/bin/env python
# -*- coding: utf-8 -*-

# 	* Copyright (C) 2013 Adiscon GmbH.
#	* This file is part of RSyslog
#	* 
#	* This script processes impstats logfiles and searches for abnormalities
#	*

import sys
import datetime
import time
import os

# Include regex definitions
import statslog_regex
from statslog_regex import *

# Set default variables
szInput = "rsyslog-stats.log"
bHelpOutput = False

# Helper variables


# Process Arguments
for arg in sys.argv: # [-4:]:
	if arg.find("--input=") != -1:
		szInput = arg[8:]
	elif arg.find("--h") != -1 or arg.find("-h") != -1 or arg.find("--help") != -1:
		bHelpOutput = True

if bHelpOutput: 
	print "\n\nStatslog-analyzer command line options:"
	print "======================================="
	print "	--input=<filename>		Contains the path and filename of your impstats logfile. "
	print "					Default is 'rsyslog-stats.log' \n"
	print "	--h / -h / --help		Displays this help message. \n"
	print "\n	Sampleline: ./statslog-analyzer.py --input=rsyslog-stats.log"
else:
	print "	Start analyzing impstats file '" + szInput+ "' \n"

	print "\n\n"
