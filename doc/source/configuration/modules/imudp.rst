******************************
imudp: UDP Syslog Input Module
******************************

.. index:: ! imudp

===========================  ===========================================================================
**Module Name:**             **imudp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via UDP.

Multiple receivers may be configured by specifying multiple input
statements.

Note that in order to enable UDP reception, Firewall rules probably
need to be modified as well. Also, SELinux may need additional rules.


Notable Features
================

- :ref:`imudp-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. index:: imudp; module parameters



Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imudp-timerequery`
     - .. include:: ../../reference/parameters/imudp-timerequery.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-schedulingpolicy`
     - .. include:: ../../reference/parameters/imudp-schedulingpolicy.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-schedulingpriority`
     - .. include:: ../../reference/parameters/imudp-schedulingpriority.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-batchsize`
     - .. include:: ../../reference/parameters/imudp-batchsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-threads`
     - .. include:: ../../reference/parameters/imudp-threads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-preservecase`
     - .. include:: ../../reference/parameters/imudp-preservecase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. index:: imudp; input parameters

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imudp-address`
     - .. include:: ../../reference/parameters/imudp-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-port`
     - .. include:: ../../reference/parameters/imudp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-ipfreebind`
     - .. include:: ../../reference/parameters/imudp-ipfreebind.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-device`
     - .. include:: ../../reference/parameters/imudp-device.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-ruleset`
     - .. include:: ../../reference/parameters/imudp-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-ratelimit-interval`
     - .. include:: ../../reference/parameters/imudp-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-ratelimit-burst`
     - .. include:: ../../reference/parameters/imudp-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-name`
     - .. include:: ../../reference/parameters/imudp-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-name-appendport`
     - .. include:: ../../reference/parameters/imudp-name-appendport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-defaulttz`
     - .. include:: ../../reference/parameters/imudp-defaulttz.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imudp-rcvbufsize`
     - .. include:: ../../reference/parameters/imudp-rcvbufsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. _imudp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener and for each worker thread.

The listener statistic is named starting with "imudp", followed by the
listener IP, a colon and port in parenthesis. For example, the counter for a
listener on port 514 (on all IPs) with no set name is called "imudp(\*:514)".

If an "inputname" is defined for a listener, that inputname is used instead of
"imudp" as statistic name. For example, if the inputname is set to "myudpinput",
that corresponding statistic name in above case would be "myudpinput(\*:514)".
This has been introduced in 7.5.3.

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup

The worker thread (in short: worker) statistic is named "imudp(wX)" where "X" is
the worker thread ID, which is a monotonically increasing integer starting at 0.
This means the first worker will have the name "imudp(w0)", the second "imudp(w1)"
and so on. Note that workers are all equal. It doesn’t really matter which worker
processes which messages, so the actual worker ID is not of much concern. More
interesting is to check how the load is spread between the worker. Also note that
there is no fixed worker-to-listener relationship: all workers process messages
from all listeners.

Note: worker thread statistics are available starting with rsyslog 7.5.5.

-  **disallowed** - total number of messages discarded due to disallowed sender

This counts the number of messages that have been discarded because they have
been received by a disallowed sender. Note that if no allowed senders are
configured (the default), this counter will always be zero.

This counter was introduced by rsyslog 8.35.0.


The following properties are maintained for each worker thread:

-  **called.recvmmsg** - number of recvmmsg() OS calls done

-  **called.recvmsg** - number of recvmsg() OS calls done

-  **msgs.received** - number of actual messages received


Caveats/Known Bugs
==================

-  Scheduling parameters are set **after** privileges have been dropped.
   In most cases, this means that setting them will not be possible
   after privilege drop. This may be worked around by using a
   sufficiently-privileged user account.

Examples
========

Example 1
---------

This sets up a UDP server on port 514:

.. code-block:: none

    module(load="imudp") # needs to be done just once
    input(type="imudp" port="514")


Example 2
---------

This sets up a UDP server on port 514 bound to device eth0:

.. code-block:: none

    module(load="imudp") # needs to be done just once
    input(type="imudp" port="514" device="eth0")


Example 3
---------

The following sample is mostly equivalent to the first one, but request
a larger rcvuf size. Note that 1m most probably will not be honored by
the OS until the user is sufficiently privileged.

.. code-block:: none

    module(load="imudp") # needs to be done just once
    input(type="imudp" port="514" rcvbufSize="1m")


Example 4
---------

In the next example, we set up three listeners at ports 10514, 10515 and
10516 and assign a listener name of "udp" to it, followed by the port
number:

.. code-block:: none

    module(load="imudp")
    input(type="imudp" port=["10514","10515","10516"]
          inputname="udp" inputname.appendPort="on")


Example 5
---------

The next example is almost equal to the previous one, but now the
inputname property will just be set to the port number. So if a message
was received on port 10515, the input name will be "10515" in this
example whereas it was "udp10515" in the previous one. Note that to do
that we set the inputname to the empty string.

.. code-block:: none

    module(load="imudp")
    input(type="imudp" port=["10514","10515","10516"]
          inputname="" inputname.appendPort="on")


Additional Information on Performance Tuning
============================================

Threads and Ports
-----------------

The maximum number of threads is a module parameter. Thus there is no direct
relation to the number of ports.

Every worker thread processes all inbound ports in parallel. To do so, it
adds all listen ports to an `epoll()` set and waits for packets to arrive. If
the system supports the `recvmmsg()` call, it tries to receive up to `batchSize`
messages at once. This reduces the number of transitions between user and
kernel space and as such overhead.

After the packages have been received, imudp processes each message and creates
input batches which are then submitted according to the config file's queue
definition. After that the a new cycle beings and imudp return to wait for
new packets to arrive.

When multiple threads are defined, each thread performs the processing
described above. All worker threads are created when imudp is started.
Each of them will individually awoken from epoll as data
is present. Each one reads as much available data as possible. With a low
incoming volume this can be inefficient in that the threads compete against
inbound data. At sufficiently high volumes this is not a problem because
multiple workers permit to read data from the operating system buffers
while other workers process the data they have read. It must be noted
that "sufficiently high volume" is not a precise concept. A single thread
can be very efficient. As such it is recommended to run impstats inside a
performance testing lab to find out a good number of worker threads. If
in doubt, start with a low number and increase only if performance
actually increases by adding threads.

A word of caution: just looking at thread CPU use is **not** a proper
way to monitor imudp processing capabilities. With too many threads
the overhead can increase, even strongly. This can result in a much higher
CPU utilization but still overall less processing capability.

Please also keep in your mind that additional input worker threads may
cause more mutex contention when adding data to processing queues.

Too many threads may also reduce the number of messages received via
a single recvmmsg() call, which in turn increases kernel/user space
switching and thus system overhead.

If **real time** priority is used it must be ensured that not all
operating system cores are used by imudp threads. The reason is that
otherwise for heavy workloads there is no ability to actually process
messages. While this may be desirable in some cases where queue settings
permit for large bursts, it in general can lead to pushback from the
queues.

For lower volumes, real time priority can increase the operating system
overhead by awaking imudp more often than strictly necessary and thus
reducing the effectiveness of `recvmmsg()`.

imudp threads and queue worker threads
--------------------------------------
There is no direct relationship between these two entities. Imudp submits
messages to the configured rulesets and places them into the respective
queues. It is then up to the queue config, and outside of the scope
or knowledge of imudp, how many queue worker threads will be spawned by
the queue in question.

Note, however, that queue worker threads and imudp input worker threads
compete for system resources. As such the combined overall value should
not overload the system. There is no strict rule to follow when sizing
overall worker numbers: for queue workers it strongly depends on how
compute-intense the workload is. For example, omfile actions need
few worker threads as they are fast. On the contrary, omelasticsearch often
waits for server replies and as such more worker threads can be beneficial.
The queue subsystem auto-tuning of worker threads should handle the
different needs in a useful way.

Additional Resources
====================

- `rsyslog video tutorial on how to store remote messages in a separate file <http://www.rsyslog.com/howto-store-remote-messages-in-a-separate-file/>`_.
-  Description of `rsyslog statistic
   counters <http://www.rsyslog.com/rsyslog-statistic-counter/>`_.
   This also describes all imudp counters.



.. toctree::
   :hidden:

   ../../reference/parameters/imudp-timerequery
   ../../reference/parameters/imudp-schedulingpolicy
   ../../reference/parameters/imudp-schedulingpriority
   ../../reference/parameters/imudp-batchsize
   ../../reference/parameters/imudp-threads
   ../../reference/parameters/imudp-preservecase
   ../../reference/parameters/imudp-address
   ../../reference/parameters/imudp-port
   ../../reference/parameters/imudp-ipfreebind
   ../../reference/parameters/imudp-device
   ../../reference/parameters/imudp-ruleset
   ../../reference/parameters/imudp-ratelimit-interval
   ../../reference/parameters/imudp-ratelimit-burst
   ../../reference/parameters/imudp-name
   ../../reference/parameters/imudp-name-appendport
   ../../reference/parameters/imudp-defaulttz
   ../../reference/parameters/imudp-rcvbufsize
