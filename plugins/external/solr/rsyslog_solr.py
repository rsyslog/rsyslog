#! /usr/bin/python

"""A simple plugin to connect rsyslog to SOLR

   Based on Radu Gheorghe's idea as expressed in
   http://blog.sematext.com/2013/12/16/video-using-solr-for-logs-with-rsyslog-flume-fluentd-and-logstash/
   Watch out for slide 26.

   Copyright (C) 2014 by Adiscon GmbH

   This file is part of rsyslog.
  
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
   
         http://www.apache.org/licenses/LICENSE-2.0
         -or-
         see COPYING.ASL20 in the source distribution
   
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
"""

import sys
import select
import pysolr

# skeleton config parameters
pollPeriod = 0.75 # the number of seconds between polling for new messages
maxAtOnce = 1024  # max nbr of messages that are processed within one batch

# App logic global variables
solrURL = "http://localhost:8983/solr/" # CONFIG: where to connect to?
solr = "" # handle to solr

def onInit():
	""" Do everything that is needed to initialize processing (e.g.
	    open files, create handles, connect to systems...)
	"""
	global solr
	solr = pysolr.Solr(solrURL)


def onReceive(msgs):
	"""This is the entry point where actual work needs to be done. It receives
	   a list with all messages pulled from rsyslog. The list is of variable
	   length, but contains all messages that are currently available. It is
	   suggest NOT to use any further buffering, as we do not know when the
	   next message will arrive. It may be in a nanosecond from now, but it
	   may also be in three hours...
	"""
	global solr
	for msg in msgs:
		# this code can most probably be improved so that multiple
		# messages are emitted to solr at once... Are you up for a
		# contribution??? ;)
		doc = json.loads(line)
		solr.add([doc])


def onExit():
	""" Do everything that is needed to finish processing (e.g.
	    close files, handles, disconnect from systems...). This is
	    being called immediately before exiting.
	"""
	global solr
	# looks like we have nothing to do here...


"""
-------------------------------------------------------
This is plumbing that DOES NOT need to be CHANGED
-------------------------------------------------------
"""
onInit()
keepRunning = 1
while keepRunning == 1:
	while keepRunning and sys.stdin in select.select([sys.stdin], [], [], pollPeriod)[0]:
		msgs = []
	        msgsInBatch = 0
		while keepRunning and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
			line = sys.stdin.readline()
			if line:
				msgs.append(line)
			else: # an empty line means stdin has been closed
				keepRunning = 0
			msgsInBatch = msgsInBatch + 1
			if msgsInBatch >= maxAtOnce:
				break;
		if len(msgs) > 0:
			onReceive(msgs)
			sys.stdout.flush() # very important, Python buffers far too much!
onExit()
