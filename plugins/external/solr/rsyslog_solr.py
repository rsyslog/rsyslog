#!/usr/bin/env python

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
import httplib
import socket
import time

# skeleton config parameters
pollPeriod = 0.75 # the number of seconds between polling for new messages
maxAtOnce = 5000  # max nbr of messages that are processed within one batch
retryInterval = 5
numberOfRetries = 10
errorFile = open("/var/log/rsyslog_solr_oopsies.log", "a")

# App logic global variables
solrServer = "localhost"
solrPort = 8983
solrUpdatePath = "/solr/gettingstarted/update"
solrConnection = "" # HTTP connection to solr

def onInit():
    """ Do everything that is needed to initialize processing (e.g.
        open files, create handles, connect to systems...)
    """
    global solrConnection
    solrConnection = httplib.HTTPConnection(solrServer, solrPort)
    

def onReceive(msgs):
    """This is the entry point where actual work needs to be done. It receives
       a list with all messages pulled from rsyslog. The list is of variable
       length, but contains all messages that are currently available. It is
       suggest NOT to use any further buffering, as we do not know when the
       next message will arrive. It may be in a nanosecond from now, but it
       may also be in three hours...
    """
    solrConnection.request("POST", solrUpdatePath, msgs, {"Content-type": "application/json"})
    response = solrConnection.getresponse()
    response.read()  # apparently we have to read the whole reply to reuse the connection
    if (response.status <> 200):
        # if there's something wrong with the payload, like a schema mismatch
        # write batch to error file and move on. Normally there's no point in retrying here
        errorFile.write("%s\n" % msgs)

def onExit():
    """ Do everything that is needed to finish processing (e.g.
        close files, handles, disconnect from systems...). This is
        being called immediately before exiting.
    """
    solrConnection.close()
    errorFile.close()

onInit()
keepRunning = 1
while keepRunning == 1:
    while keepRunning and sys.stdin in select.select([sys.stdin], [], [], pollPeriod)[0]:
        msgs = "["
            msgsInBatch = 0
        while keepRunning and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
            line = sys.stdin.readline()
            if line:
                # append the JSON array used by Solr for updates
                msgs += line
                msgs += ","
            else: # an empty line means stdin has been closed
                keepRunning = 0
            msgsInBatch = msgsInBatch + 1
            if msgsInBatch >= maxAtOnce:
                break;
        if len(msgs) > 0:
            retries = 0
            while (retries < numberOfRetries):
                try:
                    # close the JSON array used by Solr for updates
                    onReceive(msgs[:-1] + "]")
                    break
                except socket.error:
                    # retry if connection failed; it will crash with flames on other exceptions, and you will lose data. But this is something you'd normally see when you first set things up
                    time.sleep(retryInterval)
                retries += 1
            # if we failed, we write the failed batch to the error file
            if (retries == numberOfRetries):
                errorFile.write("%s\n" % msgs)
            sys.stdout.flush() # very important, Python buffers far too much!
onExit()
