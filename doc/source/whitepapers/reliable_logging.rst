How reliable should reliable logging be?
========================================
With any logging, you need to decide what you want to do if the log cannot
be written

* do you want the application to stop because it can't write a log message

or

* do you want the application to continue, but not write the log message

Note that this decision is still there even if you are not logging
remotely, your local disk partition where you are writing logs can fill up,
become read-only, or have other problems.

The RFC for syslog (dating back a couple of decades, well before rsyslog
started) specify that the application writing the log message should block
and wait for the log message to be processed. Rsyslog (like every other
modern syslog daemon) fudges this a bit and buffers the log data in RAM
rather than following the original behavior of writing the data to disk and
doing a fsync before acknowledging the log message.

If you have a problem with your output from rsyslog, your application will
keep running until rsyslog fills its queues, and then it will stop.

When you configure rsyslog to send the logs to another machine (either to
rsyslog on another machine or to some sort of database), you introduce a
significant new set of failure modes for the output from rsyslog.

You can configure the size of the rsyslog memory queues (I had one machine
dedicated to running rsyslog where I created queues large enough to use
>100G of ram for logs)

You can configure rsyslog to spill from its memory queues to disk queues
(disk assisted queue mode) when it fills its memory queues.

You can create a separate set of queues for the action that has a high
probability of failing (sending to a remote machine via TCP in this case),
but this doesn't buy you more time, it just means that other logs can
continue to be written when the remote system is down.

You can configure rsyslog to have high/low watermark levels, when the queue
fills past the high watermark, rsyslog will start discarding logs below a
specified severity, and stop doing so when it drops below the low watermark
level

For rsyslog -> \*syslog, you can use UDP for your transport so that the logs
will get dropped at the network layer if the remote system is unresponsive.

You have lots of options.

If you are really concerned with reliability, I should point out that using
TCP does not eliminate the possibility of losing logs when a remote system
goes down. When you send a message via TCP, the sender considers it sent
when it's handed to the OS to send it. The OS has a window of how much data
it allows to be outstanding (sent without acknowledgement from the remote
system), and when the TCP connection fails (due to a firewall or a remote
machine going down), the sending OS has no way to tell the application what
data what data is outstanding, so the outstanding data will be lost. This
is a smaller window of loss than UDP, which will happily keep sending your
data forever, but it's still a potential for loss. Rsyslog offers the RELP
(Reliable Event Logging Protocol), which addresses this problem by using
application level acknowledgements so no messages can get lost due to
network issues. That just leaves memory buffering (both in rsyslog and in
the OS after rsyslog tells the OS to write the logs) as potential data loss
points. Those failures will only trigger if the system crashes or rsyslog
is shutdown (and yes, there are ways to address these as well)

The reason why nothing today operates without the possibility of losing
log messages is that making the logs completely reliable absolutely kills
performance. With buffering, rsyslog can handle 400,000 logs/sec on a
low-mid range machine. With utterly reliable logs and spinning disks, this
rate drops to <100 logs/sec. With a $5K PCI SSD card, you can get up to
~4,000 logs/sec (in both cases, at the cost of not being able to use the
disk for anything else on the system (so if you do use the disk for
anything else, performance drops from there, and pretty rapidly). This is
why traditional syslog had a reputation for being very slow.

See Also
--------
* https://rainer.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html


.. _concept-model-whitepapers-reliable_logging:

Conceptual model
----------------

- Reliability is a policy choice: block the application when logging stalls or drop messages to keep workloads running.
- rsyslog decouples applications via in-memory queues, only halting writers when queues saturate.
- Disk-assisted queues extend buffer capacity and persistence but introduce latency and eventual disk exhaustion risks.
- Per-action queues isolate unreliable destinations so other actions can continue during outages.
- High/low watermarks and severity-based discard policies bound backlog growth under sustained failures.
- Transport choice defines delivery guarantees: UDP sacrifices reliability; TCP reduces loss window but can drop in-flight data; RELP adds application-level acknowledgements.
- Absolute durability is limited by OS and disk flush semantics; stronger guarantees trade away throughput by orders of magnitude.
