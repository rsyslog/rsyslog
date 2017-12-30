Receiving massive amounts of messages with high performance
===========================================================

Use Case
--------

You are receiving syslog messages via UDP and or TCP at a very high data rate.
You want to tune the system so that it can process as many messages as 
possible. All messages shall be written to a single output file.

Sample Configuration
--------------------

::

   # load required modules
   module(load="imudp" threads="2"
          timeRequery="8" batchSize="128")
   module(load="imptcp" threads="3")

   # listeners
   # repeat blocks if more listeners are needed
   # alternatively, use array syntax:
   # port=["514","515",...]
   input(type="imudp" port="514"
         ruleset="writeRemoteData")
   input(type="imptcp" port="10514"
         ruleset="writeRemoteData")

   # now define our ruleset, which also includes
   # threading and queue parameters.
   ruleset(name="writeRemoteData" 
           queue.type="fixedArray"
           queue.size="250000"
	   queue.dequeueBatchSize="4096"
	   queue.workerThreads="4"
	   queue.workerThreadMinimumMessages="60000"
          ) {
       action(type="omfile" file="/var/log/remote.log"
              ioBufferSize="64k" flushOnTXEnd="off"
	      asyncWriting="on")
   }

Notes on the suggested config
-----------------------------
It is highly suggested to use a recent enough Linux kernel that supports
the **recvmmsg()** system call. This system call improves UDP reception
speed and decreases overall system CPU utilization.

We use the **imptcp** module for tcp input, as it uses more optimal
results. Note, however, that it is only available on Linux and does
currently *not* support TLS. If **imptcp** cannot be used, use
**imtcp** instead (this will be a bit slower).

When writing to the output file, we use buffered mode. This means that
full buffers are written, but during processing file lines are not
written until the buffer is full (and thus may be delayed) and also
incomplete lines are written (at buffer boundary). When the file is closed
(rsyslogd stop or HUP), the buffer is completely flushed. As this is
a high-traffic use case, we assume that buffered mode does not cause
any concerns.

Suggested User Performance Testing
----------------------------------
Each environment is a bit different.
Depending on circumstances, the **imudp** module parameters may not be
optimal. In order to obtain best performance, it is suggested to measure
performance level with two to four threads and somewhat lower and higher
batchSize. Note that these parameters affect each other. The values given
in the config above should usually work well in *high-traffic* environments.
They are sub-optimal for low to medium traffic environments.

See Also
--------
imptcp, imtcp, imudp, ruleset()

