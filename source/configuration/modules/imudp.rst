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

   Parameter names are case-insensitive.

.. index:: imudp; module parameters


Module Parameters
-----------------

TimeRequery
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "``$UDPServerTimeRequery``"

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


SchedulingPolicy
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$IMUDPSchedulingPolicy``"

Can be used the set the scheduler priority, if the necessary
functionality is provided by the platform. Most useful to select
"fifo" for real-time processing under Linux (and thus reduce chance
of packet loss). Other options are "rr" and "other".


SchedulingPriority
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "none", "no", "``$IMUDPSchedulingPriority``"

Scheduling priority to use.


BatchSize
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "32", "no", "none"

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


Threads
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1", "no", "none"

.. versionadded:: 7.5.5

Number of worker threads to process incoming messages. These threads
are utilized to pull data off the network. On a busy system,
additional threads (but not more than there are CPUs/Cores) can help
improving performance and avoiding message loss. Note that with too
many threads, performance can suffer. There is a hard upper limit on
the number of threads that can be defined. Currently, this limit is
set to 32. It may increase in the future when massive multicore
processors become available.


PreserveCase
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

.. versionadded:: 8.37.0

This parameter is for controlling the case in fromhost.  If preservecase is set to "on", the case in fromhost is preserved.  E.g., 'Host1.Example.Org' when the message was received from 'Host1.Example.Org'.  Default to "off" for the backword compatibility.


.. index:: imudp; input parameters


Input Parameters
----------------

.. index:: imudp; address (input parameter)

Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$UDPServerAddress``"

Local IP address (or name) the UDP server should bind to. Use "*"
to bind to all of the machine's addresses.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "514", "yes", "``$UDPServerRun``"

Specifies the port the server shall listen to.. Either a single port can
be specified or an array of ports. If multiple ports are specified, a
listener will be automatically started for each port. Thus, no
additional inputs need to be configured.

Single port: Port="514"

Array of ports: Port=["514","515","10514","..."]


IpFreeBind
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "none"

.. versionadded:: 8.18.0

Manages the IP_FREEBIND option on the UDP socket, which allows binding it to
an IP address that is nonlocal or not (yet) associated to any network interface.

The parameter accepts the following values:

-  0 - does not enable the IP_FREEBIND option on the
   UDP socket. If the *bind()* call fails because of *EADDRNOTAVAIL* error,
   socket initialization fails.

-  1 - silently enables the IP_FREEBIND socket
   option if it is required to successfully bind the socket to a nonlocal address.

-  2 - enables the IP_FREEBIND socket option and
   warns when it is used to successfully bind the socket to a nonlocal address.


Device
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Bind socket to given device (e.g., eth0)

For Linux with VRF support, the Device option can be used to specify the
VRF for the Address.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "RSYSLOG_DefaultRuleset", "no", "``$InputUDPServerBindRuleset``"

Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.


RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 7.3.1

The rate-limiting interval in seconds. Value 0 turns off rate limiting.
Set it to a number of seconds (5 recommended) to activate rate-limiting.


RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10000", "no", "none"

.. versionadded:: 7.3.1

Specifies the rate-limiting burst in number of messages.


Name
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "imudp", "no", "none"

.. versionadded:: 8.3.3

Specifies the value of the inputname property. In older versions,
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


Name.appendPort
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

.. versionadded:: 7.3.9

Appends the port the the inputname property. Note that when no "name" is
specified, the default of "imudp" is used and the port is appended to
that default. So, for example, a listner port of 514 in that case
will lead to an inputname of "imudp514". The ability to append a port
is most useful when multiple ports are defined for a single input and
each of the inputnames shall be unique. Note that there currently is
no differentiation between IPv4/v6 listeners on the same port.


DefaultTZ
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

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


RcvBufSize
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "size", "none", "no", "none"

.. versionadded:: 7.3.9

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
available). (Maximum Value: 1G)


.. _imudp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener and for each worker thread.

The listener statistic is named starting with "imudp", followed followed by the
listener IP, a colon and port in parenthesis. For example, the counter for a
listener on port 514 (on all IPs) with no set name is called "imudp(\*:514)".

If an "inputname" is defined for a listener, that inputname is used instead of
"imudp" as statistic name. For example, if the inputname is set to "myudpinut",
that corresponding statistic name in above case would be "myudpinput(\*:514)".
This has been introduced in 7.5.3.

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup

The worker thread (in short: worker) statistic is named "imudp(wX)" where "X" is
the worker thread ID, which is an monotonically increasing integer starting at 0.
This means the first worker will have the name "imudp(w0″), the second "imudp(w1)"
and so on. Note that workers are all equal. It doesn’t really matter which worker
processes which messages, so the actual worker ID is not of much concern. More
interesting is to check how the load is spread between the worker. Also note that
there is no fixed worker-to-listener relationship: all workers process messages
from all listeners.

Note: worker thread statistics are available starting with rsyslog 7.5.5.

-  **disallowed** - total number of messages discarded due to disallowed sender

This counts the number of messages that have been discarded because they have
been received by an disallowed sender. Note that if no allowed senders are
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

This sets up an UDP server on port 514:

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
10516 and assign a listner name of "udp" to it, followed by the port
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


Additional Resources
====================

- `rsyslog video tutorial on how to store remote messages in a separate file <http://www.rsyslog.com/howto-store-remote-messages-in-a-separate-file/>`_.
-  Description of `rsyslog statistic
   counters <http://www.rsyslog.com/rsyslog-statistic-counter/>`_.
   This also describes all imudp counters.

