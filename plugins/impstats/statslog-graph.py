#! /usr/bin/env python
# -*- coding: utf-8 -*-

# 	* Copyright (C) 2013 Adiscon GmbH.
#	* This file is part of RSyslog
#	* 
#	* This script processes csv stats logfiles created by statslog-splitter.py and creates graphs 
#	* Dependecies:	- python pip
#	*		- pip install CairoSVG tinycss cssselect pygal
#	*

import sys
import datetime
import time
import pygal 

# Set default variables
szInput = ""
szOutputFile = "output.svg"
nLineCount = 0 
nHelpOutput = 0
nDataRecordCound = 0 

# Init variables
aFields = []
aData = {}

# Process Arguments
for arg in sys.argv[-4:]:
	if arg.find("--input=") != -1:
		szInput = arg[8:]
	elif arg.find("--outputdir=") != -1:
		szOutputFile = arg[12:]
	elif arg.find("--h") != -1 or arg.find("-h") != -1 or arg.find("--help") != -1:
		nHelpOutput = 1

if nHelpOutput == 1: 
	print "\n\nStatslog-graph command line options:"
	print "======================================="
	print "	--input=<filename>	Contains the path and filename of your impstats logfile. "
	print "				Default is 'rsyslog-stats.log' \n"
	print "	--outputfile=<dir>	Output directory and file to be used. "
	print "				Default is '" + szOutputFile + "'. "
	print "	--h / -h / --help	Displays this help message. \n"
	print "\n	Sampleline: ./statslog-graph.py --input=imuxsock.csv --outputfile=/home/user/csvgraphs/imuxsock.svg"
else:
	inputfile = open(szInput, 'r')
	for line in inputfile.readlines():
		if nLineCount == 0: 
			aFields = line.strip().split(";")
			# remove last item if empty
			if len(aFields[len(aFields)-1]) == 0: 
				aFields.pop()
			#print aFields
			#sys.exit(0)
			
			#Init data arrays
			for field in aFields:
				aData[field] = []
		else:
			aLineData = line.strip().split(";")
			# remove last item if empty
			if len(aLineData[len(aLineData)-1]) == 0: 
				aLineData.pop()

			# Loop Through line data
			iFieldNum = 0
			for field in aFields:
				if iFieldNum > 2:
					aData[field].append( int(aLineData[iFieldNum]) )
				else:
					aData[field].append( aLineData[iFieldNum] )

				iFieldNum += 1
				# print aData[field[nLineCount]] 
			
			# Increment counter
			nDataRecordCound += 1

			#print aData
			#sys.exit(0)

		# Increment counter
		nLineCount += 1
	
	# Barchart Test!
	bar_chart = pygal.Bar()                                           
	bar_chart.title = 'CSV Chart of "' + szInput + '"'
	bar_chart.x_labels = aData[ aFields[0] ] 
	bar_chart.add(aFields[3], aData[ aFields[3] ])  # Add some values
	bar_chart.render_to_file(szOutputFile)

	sys.exit(0)





line_chart = pygal.Line()
line_chart.title = 'Browser usage evolution (in %)'
line_chart.x_labels = map(str, range(2002, 2013))
line_chart.add('Firefox', [None, None, 0, 16.6,   25,   31, 36.4, 45.5, 46.3, 42.8, 37.1])
line_chart.add('Chrome',  [None, None, None, None, None, None,    0,  3.9, 10.8, 23.8, 35.3])
line_chart.add('IE',      [85.8, 84.6, 84.7, 74.5,   66, 58.6, 54.7, 44.8, 36.2, 26.6, 20.1])
line_chart.add('Others',  [14.2, 15.4, 15.3,  8.9,    9, 10.4,  8.9,  5.8,  6.7,  6.8,  7.5])
line_chart.render_to_file('bar_chart.svg')

