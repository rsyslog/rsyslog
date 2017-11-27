#!/usr/bin/env python

"""A message modification plugin to anonymize credit card numbers.

   Copyright (C) 2014 by Adiscon GmbH and Peter Slavov

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
import re
import json

# skeleton config parameters
# currently none

# App logic global variables

def onInit():
	""" Do everything that is needed to initialize processing (e.g.
	    open files, create handles, connect to systems...)
	"""
	global rc
	global patterns
	patterns = {'(^|[^A-Za-z0-9.])(34|37)[0-9]{13}([^A-Za-z0-9]|$)': 'XXXX-Amex-XXXX',           # Amex
	            '(^|[^A-Za-z0-9.])(5020|5038|6759)[0-9]{12}([^A-Za-z0-9]|$)': 'XXXX-Maes-XXXX',  # Maes
	            '(^|[^A-Za-z0-9.])4([0-9]{12}|[0-9]{15})([^A-Za-z0-9]|$)': 'XXXX-Visa-XXXX'      # Visa
                   }
	rc = re.compile("("+")|(".join(patterns.keys())+")")


def onReceive(msg):
	"""This is the entry point where actual work needs to be done. It receives
	   the messge from rsyslog and now needs to examine it, do any processing
	   necessary. The to-be-modified properties (one or many) need to be pushed
	   back to stdout, in JSON format, with no interim line breaks and a line
	   break at the end of the JSON. If no field is to be modified, empty
	   json ("{}") needs to be emitted.
	   Note that no batching takes place (contrary to the output module skeleton)
	   and so each message needs to be fully processed (rsyslog will wait for the
	   reply before the next message is pushed to this module).
	"""
	global rc
	global patterns

	def lookup(match):
		for pat in patterns.keys():
			res = re.match(pat, match.group(0))
			if res:
				return str(res.group(1))+patterns[pat]+str(res.group(res.lastindex))
				break

	res_msg = rc.sub(lambda m: lookup(m), msg)
	if res_msg == msg:
		print json.dumps({})
	else:
		print json.dumps({'msg': res_msg})

def onExit():
	""" Do everything that is needed to finish processing (e.g.
	    close files, handles, disconnect from systems...). This is
	    being called immediately before exiting.
	"""
	# most often, nothing to do here


"""
-------------------------------------------------------
This is plumbing that DOES NOT need to be CHANGED
-------------------------------------------------------
Implementor's note: Python seems to very agressively
buffer stdouot. The end result was that rsyslog does not
receive the script's messages in a timely manner (sometimes
even never, probably due to races). To prevent this, we
flush stdout after we have done processing. This is especially
important once we get to the point where the plugin does
two-way conversations with rsyslog. Do NOT change this!
See also: https://github.com/rsyslog/rsyslog/issues/22
"""
onInit()
keepRunning = 1
while keepRunning == 1:
	msg = sys.stdin.readline()
	if msg:
		msg = msg[:-1] # remove LF
		onReceive(msg)
		sys.stdout.flush() # very important, Python buffers far too much!
	else: # an empty line means stdin has been closed
		keepRunning = 0
onExit()
sys.stdout.flush() # very important, Python buffers far too much!
