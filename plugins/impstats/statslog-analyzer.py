#! /usr/bin/env python
# -*- coding: utf-8 -*-

# 	* Copyright (C) 2013-2018 Adiscon GmbH.
#	* This file is part of RSyslog
#	*
#	* This script processes impstats logfiles and searches for abnormalities
#	* Dependecies:	- python pip		-> Needed to install python packages
#	*		- python cairosvg	-> Needed for PNG converting support!
#	*		- Install python packages using this command:
#	*		pip install CairoSVG tinycss cssselect pygal
#	*

import sys
import datetime

# Include regex definitions
#import statslog_regex
from statslog_regex import *

# Set default variables
szInput = "rsyslog-stats.log"
szTmpDir = "/tmp/";
bHelpOutput = False
bDebugOutput = False	# Set to TRUE if desired!
nEvictedAlarm = 5	# In Percent, when this specific amount of requests raises the evicted counter, a problem is reported.
nFailedAlarm = 1	# Number of fails to occur until a problem is reported.
nDiscardedAlarm = 1	# Number of discards to occur until a problem is reported.

# Helper variables
nLogLineNum = 0
nLogFileCount = 0
outputData = {}

# Init result with file handles
outputFiles = {}

# ArrayID's in Logdata
LN_DATE = 0
LN_HOST = 1
LN_LINENUM = 2
LN_DATA = 3

# Open Errorlog on init
errorlog = open(szTmpDir + "statslog-analyzer.corrupted.log", 'w')

# Process Arguments
for arg in sys.argv: # [-4:]:
	if arg.find("--input=") != -1:
		szInput = arg[8:]
	elif arg.find("--evictedalarm=") != -1:
		nEvictedAlarm = int(arg[15:])
		if nEvictedAlarm < 0 or nEvictedAlarm > 100:
			nEvictedAlarm = 5 # Resett to default if value is invalid
	elif arg.find("--failedalarm=") != -1:
		nFailedAlarm = int(arg[14:])
	elif arg.find("--discardedalarm=") != -1:
		nDiscardedAlarm = int(arg[17:])
	elif arg.find("--d") != -1 or arg.find("--debug") != -1:
		bDebugOutput = True
	elif arg.find("--h") != -1 or arg.find("-h") != -1 or arg.find("--help") != -1:
		bHelpOutput = True

if bHelpOutput:
	print "\n\nStatslog-analyzer command line options:"
	print "======================================="
	print "	--input=<filename>		Contains the path and filename of your impstats logfile. "
	print "					Default is 'rsyslog-stats.log' \n"
	print "	--evictedalarm=<number>		Default is 5 which is in percent."
	print "					So the value can be between 0 and 100.\n"
	print "	--failedalarm=<number>		Number of fails that have to occur before raising an alarm.\n"
	print "	--discardedalarm=<number>	Number of discards that have to occur before raising an alarm.\n"
	print "	--d --debug			Enable verbose debug output. \n"
	print "	--h / -h / --help		Displays this help message. \n"
	print "\n	Sampleline: ./statslog-analyzer.py --input=rsyslog-stats.log --evictedalarm=5 --failedalarm=1 --discardedalarm=1"
else:
	print "	Start sorting impstats file ... "
	if bDebugOutput:
		print ": '" + szInput+ "'"
	
	# Open inout file
	inputfile = open(szInput, 'r')
	for line in inputfile.readlines():
#		if line.find("rsyslogd-pstats") != -1:

		# Init variables
		aFields = []
		aData = {}
		aDataCsv = []
		iLogRegExIndex = 0
		bLogProcessed = False

		# Loop through Regex parsers
		for loglineregex in loglineregexes:
			# Parse IMPStats Line!
			result = loglineregex.split(line)
			
			# Found valid logline, save into file!
			if len(result) >= loglineindexes[iLogRegExIndex]["LN_LOGDATA"]:
				# Get/Set SyslogTag
				if loglineindexes[iLogRegExIndex]["LN_SYSLOGTAG"] == -1:
					szLogSyslogTag = "rsyslogd-pstats" # Set to "rsyslogd-pstats" if no SyslogTag is available
				else:
					szLogSyslogTag = result[ loglineindexes[iLogRegExIndex]["LN_SYSLOGTAG"] ]

				# Get/Set SyslogTag
				if loglineindexes[iLogRegExIndex]["LN_HOST"] == -1:
					szLogHost = "localhost" # Set default to "localhost" if no host is available
				else:
					szLogHost = result[ loglineindexes[iLogRegExIndex]["LN_HOST"] ]

				if szLogSyslogTag == "rsyslogd-pstats":
					# Init StatsID!
					szStatsID = "Unknown"

					# If LogobjectID is -1, we have JSON Format in LN_LOGDATA
					if loglineindexes[iLogRegExIndex]["LN_LOGOBJECT"] == -1:
						# Remove unecessary characters and split into templ array
						aCleanedArray = re.sub("[{}\"]", "", result[loglineindexes[iLogRegExIndex]["LN_LOGDATA"]]).split(",")
						# Reset Logdata
						result[ loglineindexes[iLogRegExIndex]["LN_LOGDATA"] ] = ""
						# Loop Through temp array
						for szTempEntry in aCleanedArray:
							if szTempEntry.find("name:") != -1:
								szStatsID = szTempEntry[5:]
							else:
								# Correct and append to Logdata
								result[ loglineindexes[iLogRegExIndex]["LN_LOGDATA"] ] += re.sub("[:]", "=", szTempEntry) + " "
						#print szStatsID + " - " + result[ loglineindexes[iLogRegExIndex]["LN_LOGDATA"] ]
						#sys.exit(0)
					else:
						# Remove invalid characters from StatsID!
	#					szStatsID = re.sub("[^a-zA-Z0-9]", "_", result[ loglineindexes[iLogRegExIndex]["LN_LOGOBJECT"] ])
						szStatsID = result[ loglineindexes[iLogRegExIndex]["LN_LOGOBJECT"] ]

					# Convert Datetime!
					try:
						iMonth = int( result[ loglineindexes[iLogRegExIndex]["LN_MONTH"] ] )
						filedate = datetime.datetime.strptime(result[ loglineindexes[iLogRegExIndex]["LN_MONTH"] ] + " " + str(datetime.datetime.now().year) + " " + result[ loglineindexes[iLogRegExIndex]["LN_DAY"] ] + " " + result[ loglineindexes[iLogRegExIndex]["LN_TIME"] ] ,"%m %Y %d %H:%M:%S")
					except ValueError:
						filedate = datetime.datetime.strptime(result[ loglineindexes[iLogRegExIndex]["LN_MONTH"] ] + " " + str(datetime.datetime.now().year) + " " + result[ loglineindexes[iLogRegExIndex]["LN_DAY"] ] + " " + result[ loglineindexes[iLogRegExIndex]["LN_TIME"] ] ,"%b %Y %d %H:%M:%S")

					# Split logdata into Array
					aProperties = result[ loglineindexes[iLogRegExIndex]["LN_LOGDATA"] ].rstrip().split(" ")
					for szProperty in aProperties:
						aProperty = szProperty.split("=")
						aFields.append(aProperty[0])	# Append FieldID
						if len(aProperty) > 1:
							aDataCsv.append(aProperty[1])	# Append FieldData
							aData[aProperty[0]] = aProperty[1]
						else:
							errorlog.write("Corrupted Logline at line " + str(nLogLineNum) + " failed to parse: " + result[ loglineindexes[iLogRegExIndex]["LN_LOGDATA"] ])
							break
				
					# Open Statsdata per ID
					if szStatsID not in outputData:
						outputData[szStatsID] = []
					
					# Add data into array
					outputData[szStatsID].append( [	filedate.strftime("%Y/%b/%d %H:%M:%S"),
									szLogHost,
									nLogLineNum,
									aData ] )

					# --- Save into CSV File as well!
					# Remove invalid characters for filename!
					szFileName = szInput + "-" + re.sub("[^a-zA-Z0-9]", "_", szStatsID) + ".csv"

					# Open file for writing!
					if szFileName not in outputFiles:
						if bDebugOutput:
							print "Creating file : " + szFileName
						outputFiles[szFileName] = open(szFileName, 'w')
						nLogFileCount += 1
						
						# Output CSV Header
						outputFiles[szFileName].write("Date;")
						outputFiles[szFileName].write("Host;")
						outputFiles[szFileName].write("Object;")
						for szField in aFields:
							outputFiles[szFileName].write(szField + ";")
						outputFiles[szFileName].write("\n")
					# Output CSV Data
					outputFiles[szFileName].write(filedate.strftime("%Y/%b/%d %H:%M:%S") + ";")
					outputFiles[szFileName].write(szLogHost + ";")
					outputFiles[szFileName].write(szStatsID + ";")
					for szData in aDataCsv:
						outputFiles[szFileName].write(szData + ";")
					outputFiles[szFileName].write("\n")
					# ---

					# Set Log as processed, abort Loop!
					bLogProcessed = True
					

			# Increment Logreged Counter
			iLogRegExIndex += 1

			# Abort Loop if processed
			if bLogProcessed:
				break

		# Debug output if format not detected
		if bLogProcessed == False and bDebugOutput:
			print "Fail parsing logline: "
			print result

		# Increment helper counter
		nLogLineNum += 1

	# Close outfiles
	for outFileName in outputFiles:
		outputFiles[outFileName].close()

	# Close input file
	inputfile.close()

	# Data sorted, now analyze data!
	print "	Sorting finished with '" + str(nLogLineNum) + "' loglines processed."
	print "	Start analyzing logdata now"

	#print outputData
	#sys.exit(0)

	# Init variables
	aPossibleProblems = {}

	# Loop through data
	for szSingleStatsID in outputData:
		# Init variables
		aProblemFound = {}
		bEvictedMsgs = False
		bFailedMsgs = False
		aPossibleProblems[szSingleStatsID] = []
		iLineNum = 0
		szLineDate  = ""
		szHost = ""

		# Loop through loglines
		for singleStatLine in outputData[szSingleStatsID]:
			iLineNum = singleStatLine[LN_LINENUM]
			szLineDate = singleStatLine[LN_DATE]
			szHost = singleStatLine[LN_HOST]
			
			# Init helper values
			iLogEvictedData = 0
			iLogRequestsData = 0

			# Loop through data
			for logDataID in singleStatLine[LN_DATA]:
				if logDataID.find("discarded") != -1 or logDataID.find("failed") != -1:
					iLogData = int(singleStatLine[LN_DATA][logDataID])
					if iLogData > 0:
						if logDataID not in aProblemFound:
							# Init Values
							aProblemFound[logDataID] = {}
							aProblemFound[logDataID]['type'] = logDataID
							aProblemFound[logDataID]['value'] = iLogData
							aProblemFound[logDataID]['startline'] = iLineNum
							aProblemFound[logDataID]['startdate'] = szLineDate
							aProblemFound[logDataID]['endline'] = iLineNum
							aProblemFound[logDataID]['enddate'] = szLineDate
						elif aProblemFound[logDataID]['value'] > iLogData:
							# Logdata was reset
							aProblemFound[logDataID]['endline'] = iLineNum
							aProblemFound[logDataID]['enddate'] = szLineDate

							# Add to possible Problems Array
							if logDataID.find("discarded") != -1:
								if aProblemFound[logDataID]['value'] >= nDiscardedAlarm:
									aPossibleProblems[szSingleStatsID].append( aProblemFound[logDataID] )
							elif logDataID.find("failed") != -1:
								if aProblemFound[logDataID]['value'] >= nFailedAlarm:
									aPossibleProblems[szSingleStatsID].append( aProblemFound[logDataID] )

							# Reinit
							del aProblemFound[logDataID]
						else:
							aProblemFound[logDataID]['value'] = int(singleStatLine[LN_DATA][logDataID])
							aProblemFound[logDataID]['endline'] = iLineNum
							aProblemFound[logDataID]['enddate'] = szLineDate
				elif logDataID.find("evicted") != -1 or logDataID.find("requests") != -1:
					iLogData = int(singleStatLine[LN_DATA][logDataID])
					if "omfile" not in aProblemFound:
						# Init Value
						aProblemFound['omfile'] = {}
						aProblemFound['omfile']['type'] = "omfile"
						aProblemFound['omfile']['evicted'] = 0	# Init value
						aProblemFound['omfile']['requests'] = 0	# Init value
						aProblemFound['omfile'][logDataID] = iLogData # Set value
						aProblemFound['omfile']['startline'] = iLineNum
						aProblemFound['omfile']['startdate'] = szLineDate
						aProblemFound['omfile']['endline'] = iLineNum
						aProblemFound['omfile']['enddate'] = szLineDate
					elif aProblemFound['omfile'][logDataID] > iLogData:
						# Logdata was reset
						aProblemFound['omfile']['endline'] = iLineNum
						aProblemFound['omfile']['enddate'] = szLineDate

						# Add to possible Problems Array if exceeds alarm level	| FORCE FLOAT CALC
						if (aProblemFound['omfile']['requests'] > 0 and aProblemFound['omfile']['evicted'] / (float(aProblemFound['omfile']['requests'])/100)) >= nEvictedAlarm:
							aPossibleProblems[szSingleStatsID].append( aProblemFound['omfile'] )

						# Reinit
						del aProblemFound['omfile']
					else:
						aProblemFound['omfile'][logDataID] = int(singleStatLine[LN_DATA][logDataID])
						aProblemFound['omfile']['endline'] = iLineNum
						aProblemFound['omfile']['enddate'] = szLineDate
		# Finish possible problems array
		if len(aProblemFound) > 0:
			for szProblemID in aProblemFound:
				if aProblemFound[szProblemID]['type'].find('omfile') != -1:
					# Only add if exceeds percentage	| FORCE FLOAT CALC
					if aProblemFound[szProblemID]['requests'] > 0 and (aProblemFound[szProblemID]['evicted'] / (float(aProblemFound[szProblemID]['requests'])/100)) >= nEvictedAlarm:
						aPossibleProblems[szSingleStatsID].append( aProblemFound[szProblemID] )
				elif aProblemFound[szProblemID]['type'].find('failed') != -1:
					# Only add if exceeds Alarm Level
					if aProblemFound[szProblemID]['value'] >= nFailedAlarm:
						aPossibleProblems[szSingleStatsID].append( aProblemFound[szProblemID] )
				elif aProblemFound[szProblemID]['type'].find('discarded') != -1:
					# Only add if exceeds Alarm Level
					if aProblemFound[szProblemID]['value'] >= nDiscardedAlarm:
						aPossibleProblems[szSingleStatsID].append( aProblemFound[szProblemID] )
				else:
					# Add to problem array!
					aPossibleProblems[szSingleStatsID].append( aProblemFound[szProblemID] )

		# Debug break
#		sys.exit(0)
	

	print "	End analyzing logdata"

	print"Begin showing found problems."
	if len(aPossibleProblems) > 0:
		# Output all found problems!
		for szProblemID in aPossibleProblems:
			if len(aPossibleProblems[szProblemID]) > 0:
				print "\n	Problems found for Counter " + szProblemID + ": "
				for aProblem in aPossibleProblems[szProblemID]:
					if aProblem['type'].find("discarded") != -1:
						print "	- " + str(aProblem['value']) + " Messages were discarded (" + aProblem['type'] + ") between line " + str(aProblem['startline']) + " and " + str(aProblem['endline']) + " (Startdate: '" + str(aProblem['startdate']) + "' Enddate: '" + str(aProblem['enddate']) + "') "
					elif aProblem['type'].find("failed") != -1:
						print "	- " + str(aProblem['value']) + " Messages failed (" + aProblem['type'] + ") between line " + str(aProblem['startline']) + " and " + str(aProblem['endline']) + " (Startdate: '" + str(aProblem['startdate']) + "' Enddate: '" + str(aProblem['enddate']) + "') "
					elif aProblem['type'].find("omfile") != -1:
						nEvictedInPercent = aProblem['evicted'] / (aProblem['requests']/100)
						print "	- " + str(nEvictedInPercent) + "% File Requests were evicted ('" + str(aProblem['requests']) + "' requests, '" + str(aProblem['evicted']) + "' evicted) between line " + str(aProblem['startline']) + " and " + str(aProblem['endline']) + " (Startdate: '" + str(aProblem['startdate']) + "' Enddate: '" + str(aProblem['enddate']) + "') "
	print"End showing found problems."

	# Finished
	print "\n\n"
