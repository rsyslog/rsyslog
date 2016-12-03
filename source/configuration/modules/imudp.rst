imudp: UDP Syslog Input Module
==============================

.. index:: ! imudp 

===========================  ===========================================================================
**Module Name:**             **imudp**
**Author:**                  `Rainer Gerhards <http://www.gerhards.net/rainer>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

Provides the ability to receive syslog messages via UDP.

Multiple receivers may be configured by specifying multiple input
statements.

Note that in order to enable UDP reception, Firewall rules probably
need to be modified as well. Also, SELinux may need additional rules.

Configuration Parameters
------------------------

.. index:: imudp; module parameters

Module Parameters
^^^^^^^^^^^^^^^^^

.. function::  TimeRequery <nbr-of-times>

   *Default: 2*

   This is a performance optimization. Getting the system time is very
   costly. With this setting, imudp can be instructed to obtain the
   precise time only once every n-times. This logic is only activated if
   messages come in at a very fast rate, so doing less frequent time
   calls should usually be acceptable. The default value is two, because
   we have seen that even without optimization the kernel often returns
   twice the identical time. You can set this value as high as you like,
   but do so at your own risk. The higher the value, the less precise
   the timestamp.

   **Note:** the timeRequery is done based on executed system calls
   (**not** messages received). So when batch sizes are used, multiple
   messages are received with one system call. All of these messages
   always receive the same timestamp, as they are effectively received
   at the same time. When there is very high traffic and successive
   system calls immediately return the next batch of messages, the time
   requery logic kicks in, which means that by default time is only
   queried for every second batch. Again, this should not cause a
   too-much deviation as it requires messages to come in very rapidly.
   However, we advise not to set the "timeRequery" parameter to a large
   value (larger than 10) if input batches are used.

.. function::  SchedulingPolicy <rr/fifo/other>

   *Default: unset*

   Can be used the set the scheduler priority, if the necessary
   functionality is provided by the platform. Most useful to select
   "fifo" for real-time processing under Linux (and thus reduce chance
   of packet loss).

.. function::  SchedulingPriority <number>

   *Default: unset*

   Scheduling priority to use.

.. function::  batchSize <number>

   *Default: 32*

   This parameter is only meaningful if the system support recvmmsg()
   (newer Linux OSs do this). The parameter is silently ignored if the
   system does not support it. If supported, it sets the maximum number
   of UDP messages that can be obtained with a single OS call. For
   systems with high UDP traffic, a relatively high batch size can
   reduce system overhead and improve performance. However, this
   parameter should not be overdone. For each buffer, max message size
   bytes are statically required. Also, a too-high number leads to
   reduced efficiency, as some structures need to be completely
   initialized before the OS call is done. We would suggest to not set
   it above a value of 128, except if experimental results show that
   this is useful.

.. function::  threads <number>

   *Available since: 7.5.5*

   *Default: 1*

   Number of worker threads to process incoming messages. These threads
   are utilized to pull data off the network. On a busy system,
   additional threads (but not more than there are CPUs/Cores) can help
   improving performance and avoiding message loss. Note that with too
   many threads, performance can suffer. There is a hard upper limit on
   the number of threads that can be defined. Currently, this limit is
   set to 32. It may increase in the future when massive multicore
   processors become available.

.. index:: imudp; input parameters

Input Parameters
^^^^^^^^^^^^^^^^

..index:: imudp; address (input parameter)

.. function::  Address <IP>

   *Default: \**

   Local IP address (or name) the UDP server should bind to. Use \"*"
   to bind to all of the machine's addresses.

.. index:: 
   single: imudp; port (input parameter)
.. function::  Port <port>

   *Default: 514*

   Specifies the port the server shall listen to.. Either a single port can
   be specified or an array of ports. If multiple ports are specified, a
   listener will be automatically started for each port. Thus, no
   additional inputs need to be configured.

   Single port: Port="514"

   Array of ports: Port=["514","515","10514","..."]

.. function::  Ruleset <ruleset>

   *Default: RSYSLOG_DefaultRuleset*

   Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

.. function::  RateLimit.Interval [number]
   
   *Available since: 7.3.1*

   *Default: 0*

   The rate-limiting interval in seconds. Value 0 turns off rate limiting.
   Set it to a number of seconds (5 recommended) to activate rate-limiting.

.. function::  RateLimit.Burst [number]

   *Available since: 7.3.1*

   *Default: 10000*

   Specifies the rate-limiting burst in number of messages.

.. function::  name [name]

   *Available since: 8.3.3*

   *Default: imudp*

   specifies the value of the inputname property. In older versions,
   this was always "imudp" for all
   listeners, which still is the default. Starting with 7.3.9 it can be
   set to different values for each listener. Note that when a single
   input statement defines multipe listner ports, the inputname will be
   the same for all of them. If you want to differentiate in that case,
   use "name.appendPort" to make them unique. Note that the
   "name" parameter can be an empty string. In that case, the
   corresponding inputname property will obviously also be the empty
   string. This is primarily meant to be used together with
   "name.appendPort" to set the inputname equal to the port.

.. function::  InputName [name]

   *Available since: 7.3.9*

   **Deprecated**

   This provides the same functionality as "name". It is the historical
   parameter name and should not be used in new configurations. It is
   scheduled to be removed some time in the future.

.. function::  name.appendPort [on/off]

   *Available since: 7.3.9*

   *Default: off*

   Appends the port the the inputname property. Note that when no "name" is
   specified, the default of "imudp" is used and the port is appended to
   that default. So, for example, a listner port of 514 in that case
   will lead to an inputname of "imudp514". The ability to append a port
   is most useful when multiple ports are defined for a single input and
   each of the inputnames shall be unique. Note that there currently is
   no differentiation between IPv4/v6 listeners on the same port.

.. function::  InputName.AppendPort [on/off]

   *Available since: 7.3.9*

   **Deprecated**

   This provides the same functionality as "name.appendPort". It is the historical
   parameter name and should not be used in new configurations. It is
   scheduled to be removed some time in the future.

.. function::  defaultTZ <timezone-info>

   *Default: unset*

   This is an **experimental** parameter; details may change at any
   time and it may also be discoutinued without any early warning.
   Permits to set a default timezone for this listener. This is useful
   when working with legacy syslog (RFC3164 et al) residing in different
   timezones. If set it will be used as timezone for all messages **that
   do not contain timezone info**. Currently, the format **must** be
   "+/-hh:mm", e.g. "-05:00", "+01:30". Other formats, including TZ
   names (like EST) are NOT yet supported. Note that consequently no
   daylight saving settings are evaluated when working with timezones.
   If an invalid format is used, "interesting" things can happen, among
   them malformed timestamps and rsyslogd segfaults. This will obviously
   be changed at the time this feature becomes non-experimental.

.. function::  rcvbufSize [size]

   *Available since: 7.5.3*

   *Default: unset*

   *Maximum Value: 1G*

   This request a socket receive buffer of specific size from the operating system. It
   is an expert parameter, which should only be changed for a good reason.
   Note that setting this parameter disables Linux auto-tuning, which
   usually works pretty well. The default value is 0, which means "keep
   the OS buffer size unchanged". This is a size value. So in addition
   to pure integer values, sizes like "256k", "1m" and the like can be
   specified. Note that setting very large sizes may require root or
   other special privileges. Also note that the OS may slightly adjust
   the value or shrink it to a system-set max value if the user is not
   sufficiently privileged. Technically, this parameter will result in a
   setsockopt() call with SO\_RCVBUF (and SO\_RCVBUFFORCE if it is
   available).

See Also
--------

- `rsyslog video tutorial on how to store remote messages in a separate file <http://www.rsyslog.com/howto-store-remote-messages-in-a-separate-file/>`_.
-  Description of `rsyslog statistic
   counters <http://www.rsyslog.com/rsyslog-statistic-counter/>`_.
   This also describes all imudp counters.

Caveats/Known Bugs
------------------

-  Scheduling parameters are set **after** privileges have been dropped.
   In most cases, this means that setting them will not be possible
   after privilege drop. This may be worked around by using a
   sufficiently-privileged user account.

Samples
-------

This sets up an UPD server on port 514:

::

    module(load="imudp") # needs to be done just once
    input(type="imudp" port="514")

The following sample is mostly equivalent to the first one, but request
a larger rcvuf size. Note that 1m most probably will not be honored by
the OS until the user is sufficiently privileged.

::

    module(load="imudp") # needs to be done just once
    input(type="imudp" port="514" rcvbufSize="1m")

In the next example, we set up three listeners at ports 10514, 10515 and
10516 and assign a listner name of "udp" to it, followed by the port
number:

::

    module(load="imudp")
    input(type="imudp" port=["10514","10515","10516"]
          inputname="udp" inputname.appendPort="on")

The next example is almost equal to the previous one, but now the
inputname property will just be set to the port number. So if a message
was received on port 10515, the input name will be "10515" in this
example whereas it was "udp10515" in the previous one. Note that to do
that we set the inputname to the empty string.

::

    module(load="imudp")
    input(type="imudp" port=["10514","10515","10516"]
          inputname="" inputname.appendPort="on")

Legacy Configuration Directives
-------------------------------

Legacy configuration parameters should **not** be used when crafting new
configuration files. It is easy to get things wrong with them.

====================================== ==================== =======================
Directive                              Equivalent Parameter Requires
====================================== ==================== =======================
$UDPServerTimeRequery <nbr-of-times>   *TimeRequery*
$IMUDPSchedulingPolicy <rr/fifo/other> *SchedulingPolicy*   4.7.4+, 5.7.3+, 6.1.3+
$IMUDPSchedulingPriority <number>      *SchedulingPriority* 4.7.4+, 5.7.3+, 6.1.3+
$UDPServerAddress <IP>                 Address              
$UDPServerRun <port>                   Port
$InputUDPServerBindRuleset <ruleset>   Ruleset              5.3.2+
====================================== ==================== =======================

Note: module parameters are given in *italics*. All others are input parameters.

Multiple receivers may be configured by specifying $UDPServerRun
multiple times.

Legacy Sample
^^^^^^^^^^^^^

This sets up an UDP server on port 514:

::

   $ModLoad imudp # needs to be done just once
   $UDPServerRun 514

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2009-2014 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
