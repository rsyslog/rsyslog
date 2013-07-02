#! /usr/bin/env python
# -*- coding: utf-8 -*-

# 	* Copyright (C) 2013 Adiscon GmbH.
#	* This file is part of RSyslog
#	* 
#	* This script processes impstats logfiles and splits them into csv files
#	*

import sys
import datetime
import time
import re

# Set default variables
szInput = "rsyslog-stats.log"
szOutputDir = "./"
nSingleObjectOutput = 1
nHelpOutput = 0
nLogLineNum = 0
nLogFileCount = 0

# Create regex for logline
loglineregex = re.compile(r"(...)(?:.|..)([0-9]{1,2}) ([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}) ([a-zA-Z0-9_\-\.]{1,256}) ([A-Za-z0-9_\-\/\.]{1,32}): (.*?): (.*?) \n")
# Array Indexes
LN_MONTH = 1;LN_DAY = 2;LN_TIME = 3;LN_HOST = 4;LN_SYSLOGTAG = 5;LN_LOGOBJECT = 6;LN_LOGDATA = 7

# Init result with file handles
outputFiles = {}

# Open Errorlog on init
errorlog = open("statslog-splitter.corrupted.log", 'w')

# Process Arguments
for arg in sys.argv[-4:]:
	if arg.find("--input=") != -1:
		szInput = arg[8:]
	elif arg.find("--outputdir=") != -1:
		szOutputDir = arg[12:]
	elif arg.find("--h") != -1 or arg.find("-h") != -1 or arg.find("--help") != -1:
		nHelpOutput = 1

#sys.exit(0)

if nHelpOutput == 1: 
	print "\n\nStatslog-splitter command line options:"
	print "======================================="
	print "	--input=<filename>	Contains the path and filename of your impstats logfile. "
	print "				Default is 'rsyslog-stats.log' \n"
	print "	--outputdir=<dir>		Output directory to be used. "
	print "				Default is current directory. "
	print "	--h / -h / --help	Displays this help message. \n"
	print "	singlefile		Splits the stats logfile into single CSV Files"
	print "				(Default)"
	print "\n	Sampleline: ./statslog-splitter.py singlefile --input=rsyslog-stats.log --outputdir=/home/user/csvlogs/"
elif nSingleObjectOutput == 1:
	inputfile = open(szInput, 'r')
	for line in inputfile.readlines():
		if line.find("rsyslogd-pstats") != -1:
			# Init variables
			aFields = []
			aData = []

			# Parse IMPStats Line!
			result = loglineregex.split(line)
			# Found valid logline, save into file! 
			if len(result) >= LN_LOGDATA and result[LN_SYSLOGTAG] == "rsyslogd-pstats":
				# Convert Datetime!
				filedate = datetime.datetime.strptime(result[LN_MONTH] + " " + str(datetime.datetime.now().year) + " " + result[LN_DAY] + " " + result[LN_TIME] ,"%b %Y %d %H:%M:%S")

				# Split logdata into Array
				aProperties = result[LN_LOGDATA].split(" ")
				for szProperty in aProperties:
					aProperty = szProperty.split("=")
					aFields.append(aProperty[0])	# Append FieldID
					if len(aProperty) > 1:
						aData.append(aProperty[1])	# Append FieldData
					else: 
						errorlog.write("Corrupted Logline at line " + str(nLogLineNum) + " failed to parse: " + line)
						break

				# Remove invalid characters for filename!
				szFileName = re.sub("[^a-zA-Z0-9]", "_", result[LN_LOGOBJECT]) + ".csv"

				# Open file for writing!
				if szFileName not in outputFiles:
       					print "Creating file : " + szOutputDir + "/" + szFileName
					outputFiles[szFileName] = open(szOutputDir + "/" + szFileName, 'w')
					nLogFileCount += 1
					
					# Output CSV Header
					outputFiles[szFileName].write("Date;")
					outputFiles[szFileName].write("Host;")
					outputFiles[szFileName].write("Object;")
					for szField in aFields:
						outputFiles[szFileName].write(szField + ";")
					outputFiles[szFileName].write("\n")
				#else:
				#	print "already open: " + szFileName
				
				# Output CSV Data
				outputFiles[szFileName].write(filedate.strftime("%Y/%b/%d %H:%M:%S") + ";")
				outputFiles[szFileName].write(result[LN_HOST] + ";")
				outputFiles[szFileName].write(result[LN_LOGOBJECT] + ";")
				for szData in aData:
					outputFiles[szFileName].write(szData + ";")
				outputFiles[szFileName].write("\n")

				#print result[LN_LOGOBJECT]
				#print result[LN_LOGDATA]
			else:
				print "Fail parsing logline: "
				print result
				break


			# Increment helper counter
			nLogLineNum += 1 

			#print result
			#break

			#print "IMPStats Line found: " + line

	# Close input file
	inputfile.close()

	print "\n	File " + szInput + " has been processed"
	print "	" + str(nLogFileCount) + " Logfiles have been exported to " + szOutputDir
	print "\n\n"

# Close Error log on exit
errorlog.close()
