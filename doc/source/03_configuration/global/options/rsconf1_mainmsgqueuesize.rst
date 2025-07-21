$MainMsgQueueSize
-----------------

**Type:** global configuration parameter

**Default:** 50000 (v8.30.0) - may change

**Description:**

This allows to specify the maximum size of the message queue. This
parameter is only available when rsyslogd has been compiled with
multithreading support. In this mode, receiver and output modules are
de-coupled via an in-memory queue. This queue buffers messages when the
output modules are not capable to process them as fast as they are
received. Once the queue size is exhausted, messages will be dropped.
The slower the output (e.g. MariaDB/MySQL), the larger the queue should 
be. Buffer space for the actual queue entries is allocated on an as-needed
basis. Please keep in mind that a very large queue may exhaust available
system memory and swap space. Keep this in mind when configuring the max
size. The actual size of a message depends largely on its content and
the originator. As a rule of thumb, typically messages should not take
up more then roughly 1k (this is the memory structure, not what you see
in a network dump!). For typical linux messages, 512 bytes should be a
good bet. Please also note that there is a minimal amount of memory
taken for each queue entry, no matter if it is used or not. This is one
pointer value, so on 32bit systems, it should typically be 4 bytes and
on 64bit systems it should typically be 8 bytes. For example, the
default queue size of 10,000 entries needs roughly 40k fixed overhead on
a 32 bit system.

**Sample:**

``$MainMsgQueueSize 100000 # 100,000 may be a value to handle burst traffic``

