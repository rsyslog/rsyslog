#!/usr/bin/python3
# a small url encoder for testbench purposes
# written 2018-11-05 by Rainer Gerhards
# part of the rsyslog testbench, released under ASL 2.0
import sys
import urllib

if len(sys.argv) != 2:
	print "ERROR: urlencode needs exactly one string as argument"
	sys.exit(1)
print urllib.quote_plus(sys.argv[1])
