#! /usr/bin/env python
# -*- coding: utf-8 -*-

# 	* Copyright (C) 2013-2018 Adiscon GmbH.
#	* This file is part of RSyslog
#	*
#	* This script processes csv stats logfiles created by statslog-splitter.py and creates graphs
#	* Dependecies:	- python pip		-> Needed to install python packages
#	*		- python cairosvg	-> Needed for PNG converting support!
#	*		- Install python packages using this command:
#	*		pip install CairoSVG tinycss cssselect pygal
#	*

import sys
import datetime
import time
#import os
import pygal

# Set default variables
szInput = ""
szOutputFile = ""
szChartName = ""
szCounters = ""
bHelpOutput = False
nMaxDataCount = 25
bUseDateTime = True
bLineChart = True
bBarChart = False
bConvertPng = False
bLogarithmicChart = False
bFilledLineChart = False
bChartCalcDelta = False
bDeltaIgnoreFirstValue = False
bDeltaIgnoreNegativeValues = False

# Init variables
aFields = []
aData = {}
aMajorXData = []
aCounters = None

# Helper variables
nDataRecordCound = 0
nLineCount = 0
iStartSeconds = 0


# Process Arguments
for arg in sys.argv: # [-4:]:
	if arg.find("--input=") != -1:
		szInput = arg[8:]
	elif arg.find("--outputfile=") != -1:
		szOutputFile = arg[13:]
	elif arg.find("--maxdataxlabel=") != -1:
		nMaxDataCount = int(arg[16:])
	elif arg.find("--xlabeldatetime") != -1:
		bUseDateTime = True
	elif arg.find("--xlabelseconds") != -1:
		bUseDateTime = False
	elif arg.find("--convertpng") != -1:
		bConvertPng = True
	elif arg.find("--deltaignorefirstvalue") != -1:
		bDeltaIgnoreFirstValue = True
	elif arg.find("--deltaignorenegativevalues") != -1:
		bDeltaIgnoreNegativeValues = True
	elif arg.find("--linechart") != -1:
		bLineChart = True
		bBarChart = False
	elif arg.find("--barchart") != -1:
		bLineChart = False
		bBarChart = True
	elif arg.find("--logarithmic") != -1:
		bLogarithmicChart = True
	elif arg.find("--filledlinechart") != -1:
		bFilledLineChart = True
	elif arg.find("--chartscalcdelta") != -1:
		bChartCalcDelta = True
	elif arg.find("--counters=") != -1:
		szCounters = arg[11:]
	elif arg.find("--h") != -1 or arg.find("-h") != -1 or arg.find("--help") != -1:
		bHelpOutput = True

if bHelpOutput == True:
	print "\n\nStatslog-graph command line options:"
	print "======================================="
	print "	--input=<filename>	Contains the path and filename of your impstats logfile. "
	print "				Default is 'rsyslog-stats.log' \n"
	print "	--outputfile=<dir>	Output directory and file to be used. "
	print "				Default is '" + szOutputFile + "'. "
	print "	--maxdataxlabel=<num>	Max Number of data shown on the x-label."
	print "				Default is 25 label entries."
	print " --xlabeldatetime	Use Readable Datetime for x label data. (Cannot be used with --xlabelseconds)"
	print "				Default is enabled."
	print " --xlabelseconds		Use seconds instead of datetime, starting at 0. (Cannot be used with --xlabeldatetime)"
	print "				Default is disabled."
	print " --linechart		Generates a Linechart (Default chart mode) (Cannot be used with --barchart)"
	print " --barchart		Generates a Barchart (Cannot be used with --linechart)"
	print " --logarithmic		Uses Logarithmic to scale the Y Axis, maybe useful in some cases. Default is OFF"
	print " --filledlinechart	Use filled lines on Linechart, maybe useful in some cases. Default is OFF"
	print " --chartscalcdelta	If set, charts will use calculated delta values instead of cumulative values."
	print " --convertpng		Generate PNG Output rather than SVG. "
	print "				Default is SVG output."
	print "	--h / -h / --help	Displays this help message. \n"
	print "\n	Sampleline: ./statslog-graph.py --input=imuxsock.csv --outputfile=/home/user/csvgraphs/imuxsock.svg"
else:
	# Generate output filename
	if len(szInput) > 0:
		# Only set output filename if not specified
		if len(szOutputFile) == 0:
			if szInput.rfind(".") == -1:
				szOutputFile += szInput + ".svg"
			else:
				szOutputFile += szInput[:-4] + ".svg"
	else:
		print "Error, no input file specified!"
		sys.exit(0)

	# Generate Counters Array if necessary
	if len(szCounters) > 0:
		aCounters = szCounters.strip().split(";")

	# Open Errorlog on init
	errorlog = open(szOutputFile + ".corrupted.log", 'w')

	# Process inputfile
	with open(szInput) as inputfile:
		aLineDataPrev = [] # Helper variable for previous line!
		for line in inputfile:
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
					if iFieldNum == 0:
						if bUseDateTime:
							aData[field].append( datetime.datetime.strptime(aLineData[iFieldNum],"%Y/%b/%d %H:%M:%S") )
						else:
							# Convert Time String into UNIX Timestamp
							myDateTime = datetime.datetime.strptime(aLineData[iFieldNum],"%Y/%b/%d %H:%M:%S")
							iTimeStamp = int(time.mktime(myDateTime.timetuple()))

							# Init Start Seconds
							if iStartSeconds == 0:
								iStartSeconds = iTimeStamp

							# Set data field
							aData[field].append( iTimeStamp - iStartSeconds )
					elif iFieldNum == 2 and szChartName == "":
						szChartName = aLineData[iFieldNum]
					elif iFieldNum > 2:
						# Check if we need to calculate Deltas!
						if bChartCalcDelta and len(aLineDataPrev) > 0 and field.find("size") == -1: # Do not calc delta für SIZE!
							try:
								iPreviousVal = int(aLineDataPrev[iFieldNum])
								iCurrentVal = int(aLineData[iFieldNum])
								if iCurrentVal != 0:	# Calc DELTA
									iCurrentDelta = iCurrentVal - iPreviousVal
									if bDeltaIgnoreNegativeValues:
										if iCurrentDelta >= 0:
											aData[field].append(iCurrentDelta)
										else:
											aData[field].append(0)
									else:
										aData[field].append(iCurrentDelta)
								else:	# Don't Calc delta value!
									aData[field].append( iCurrentVal )
							except ValueError:
								errorlog.write("Error, Field not an INTEGER: " + aLineData[iFieldNum])
						else:
							try:			# Try as INTEGER
								aData[field].append( int(aLineData[iFieldNum]) )
							except ValueError:	# Not an INTEGER, skip value
								errorlog.write("Error, Field not an INTEGER: " + aLineData[iFieldNum])
					else:
						aData[field].append( aLineData[iFieldNum] )

					iFieldNum += 1
					# print aData[field[nLineCount]]
				
				# Increment counter
				nDataRecordCound += 1

				# in case deltas need to be calculated, Store current line into previous line
				if bChartCalcDelta:
					aLineDataPrev = aLineData

			# Increment counter
			nLineCount += 1

	#		if nLineCount > 25:
	#			break

	#	if nMaxDataCount > 0:
	#		# Check if we need to reduce the data amount
	#		nTotalDataCount = len( aData[aFields[0]] )
	#		nDataStepCount = nTotalDataCount / (nMaxDataCount)
	#		if nTotalDataCount > nMaxDataCount:
	#			for iDataNum in reversed(range(0, nTotalDataCount)):
	#				# Remove all entries who
	#				if iDataNum % nDataStepCount == 0:
	#					aMajorXData.append( aData[aFields[0]][iDataNum] )
			
	# Import Style
#	from pygal.style import LightSolarizedStyle
#	from pygal.style import DefaultStyle
	from pygal.style import RotateStyle

	# Create Config object
	from pygal import Config
	chartCfg = Config()
	chartCfg.show_legend = True
	chartCfg.human_readable = True
	chartCfg.pretty_print=True
	if bFilledLineChart:
		chartCfg.fill = True
	else:
		chartCfg.fill = False
	chartCfg.x_scale = 1
	chartCfg.y_scale = 1
	chartCfg.x_label_rotation = 45
	chartCfg.include_x_axis = True
	chartCfg.show_dots=False
	if nMaxDataCount > 0:
		chartCfg.show_minor_x_labels=False
		chartCfg.x_labels_major_count=nMaxDataCount
	#chartCfg.style = DefaultStyle #LightSolarizedStyle
	chartCfg.style = RotateStyle('#FF0000')
	chartCfg.print_values = True #False
	chartCfg.print_zeroes = True
	chartCfg.no_data_text = "All values are 0"
	if bLogarithmicChart:
		chartCfg.logarithmic=True	# Makes chart Y-Axis data more readable
	
	# remove first item if configured!
	if bDeltaIgnoreFirstValue:
		for iChartNum in range(3, len(aFields) ):
			if aCounters is None or aFields[iChartNum] in aCounters:
				aData[ aFields[iChartNum] ].pop(0)

	# Create Linechart
	if bLineChart:
		myChart = pygal.Line(chartCfg)
		myChart.title = 'Line Chart of "' + szChartName + '"'
		myChart.x_title = "Time elasped in seconds"
		myChart.x_labels = map(str, aData[aFields[0]] )
#		myChart.x_labels_major = map(str, aMajorXData )
		for iChartNum in range(3, len(aFields) ):
			if aCounters is None or aFields[iChartNum] in aCounters:
				myChart.add(aFields[iChartNum], aData[ aFields[iChartNum] ])  # Add some values
	# Create BarChart
	elif bBarChart:
		myChart = pygal.Bar(chartCfg)
		myChart.title = 'Bar Chart of "' + szChartName + '"'
		myChart.x_title = "Time elasped in seconds"
		myChart.x_labels = map(str, aData[aFields[0]] )
#		myChart.x_labels_major = map(str, aMajorXData )
		for iChartNum in range(3, len(aFields) ):
			if aCounters is None or aFields[iChartNum] in aCounters:
				myChart.add(aFields[iChartNum], aData[ aFields[iChartNum] ])  # Add some values
#		for iChartNum in range(3, len(aFields) ):
#			myChart.add(aFields[iChartNum], aData[ aFields[iChartNum] ])  # Add some values

	# Render Chart now and output to file!
	if bConvertPng:
		szPngFileName = szOutputFile[:-4] + ".png"
		myChart.render_to_png(szPngFileName)
	else:
		myChart.render_to_file(szOutputFile)

	# Finished
	sys.exit(0)
