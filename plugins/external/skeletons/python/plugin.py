#! /usr/bin/python

"""A skeleton for a python rsyslog output plugin
"""

pollPeriod = 0.75 # the number of seconds between polling for new messages

import sys
import select

outfile = "" # "define" global var that the app code needs

def onInit():
	""" Do everything that is needed to initialize processing (e.g.
	    open files, create handles, connect to systems...)
	"""
	global outfile
	outfile = open("/tmp/logfile", "w")


def onReceive(msgs):
	"""This is the entry point where actual work needs to be done. It receives
	   a list with all messages pulled from rsyslog. The list is of variable
	   length, but contains all messages that are currently available. It is
	   suggest NOT to use any further buffering, as we do not know when the
	   next message will arrive. It may be in a nanosecond from now, but it
	   may also be in three hours...
	"""
	global outfile
	for msg in msgs:
		outfile.write(msg)


def onExit():
	""" Do everything that is needed to finish processing (e.g.
	    close files, handles, disconnect from systems...). This is
	    being called immediately before exiting.
	"""
	global outfile
	outfile.close()


"""
-------------------------------------------------------
This is plumbing that DOES NOT need to be CHANGED
-------------------------------------------------------
"""
onInit()
keepRunning = 1
while keepRunning == 1:
	msgs = []
	while keepRunning and sys.stdin in select.select([sys.stdin], [], [], pollPeriod)[0]:
		while keepRunning and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
			line = sys.stdin.readline()
			if line:
				msgs.append(line)
			else: # an empty line means stdin has been closed
				print('eof')
				keepRunning = 0
		if len(msgs) > 0:
			onReceive(msgs)
onExit()
