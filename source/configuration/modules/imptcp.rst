************************
imptcp: Plain TCP Syslog
************************

===========================  ===========================================================================
**Module Name:**             **imptcp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via plain TCP syslog.
This is a specialized input plugin tailored for high performance on
Linux. It will probably not run on any other platform. Also, it does not
provide TLS services. Encryption can be provided by using
`stunnel <rsyslog_stunnel.html>`_.

This module has no limit on the number of listeners and sessions that
can be used.


Notable Features
================

- :ref:`imptcp-statistic-counter`
- :ref:`error-messages`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

Threads
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "``$InputPTCPServerHelperThreads``"

Number of helper worker threads to process incoming messages. These
threads are utilized to pull data off the network. On a busy system,
additional helper threads (but not more than there are CPUs/Cores)
can help improving performance. The default value is two, which means
there is a default thread count of three (the main input thread plus
two helpers). No more than 16 threads can be set (if tried to,
rsyslog always resorts to 16).


MaxSessions
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Maximum number of open sessions allowed.  This is inherited to each
"input()" config, however it is not a global maximum, rather just
setting the default per input.

A setting of zero or less than zero means no limit.

ProcessOnPoller
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Instructs imptcp to process messages on poller thread opportunistically.
This leads to lower resource footprint(as poller thread doubles up as
message-processing thread too). "On" works best when imptcp is handling
low ingestion rates.

At high throughput though, it causes polling delay(as poller spends time
processing messages, which keeps connections in read-ready state longer
than they need to be, filling socket-buffer, hence eventually applying
backpressure).

It defaults to allowing messages to be processed on poller (for backward
compatibility).


Input Parameters
----------------

These parameters can be used with the "input()" statement. They apply to
the input they are specified with.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputPTCPServerRun``"

Select a port to listen on. It is an error to specify
both `path` and `port`.


Path
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

A path on the filesystem for a unix domain socket. It is an error to specify
both `path` and `port`.


DiscardTruncatedMsg
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When a message is split because it is to long the second part is normally
processed as the next message. This can cause Problems. When this parameter
is turned on the part of the message after the truncation will be discarded.

FileOwner
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "UID", "system default", "no", "none"

Set the file owner for the domain socket. The
parameter is a user name, for which the userid is obtained by
rsyslogd during startup processing. Interim changes to the user
mapping are *not* detected.


FileOwnerNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "none"

Set the file owner for the domain socket. The
parameter is a numerical ID, which is used regardless of
whether the user actually exists. This can be useful if the user
mapping is not available to rsyslog during startup.


FileGroup
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "GID", "system default", "no", "none"

Set the group for the domain socket. The parameter is
a group name, for which the groupid is obtained by rsyslogd during
startup processing. Interim changes to the user mapping are not
detected.


FileGroupNum
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "system default", "no", "none"

Set the group for the domain socket. The parameter is
a numerical ID, which is used regardless of whether the group
actually exists. This can be useful if the group mapping is not
available to rsyslog during startup.


FileCreateMode
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "octalNumber", "0644", "no", "none"

Set the access permissions for the domain socket. The value given must
always be a 4-digit octal number, with the initial digit being zero.
Please note that the actual permission depend on rsyslogd's process
umask. If in doubt, use "$umask 0000" right at the beginning of the
configuration file to remove any restrictions.


FailOnChOwnFailure
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Rsyslog will not start if this is on and changing the file owner, group,
or access permissions fails. Disable this to ignore these errors.


Unlink
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If a unix domain socket is being used this controls whether or not the socket
is unlinked before listening and after closing.


Name
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "imptcp", "no", "``$InputPTCPServerInputName``"

Sets a name for the inputname property. If no name is set "imptcp"
is used by default. Setting a name is not strictly necessary, but can
be useful to apply filtering based on which input the message was
received from. Note that the name also shows up in
:doc:`impstats <impstats>` logs.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputPTCPServerBindRuleset``"

Binds specified ruleset to this input. If not set, the default
ruleset is bound.


MaxFrameSize
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200000", "no", "none"

When in octet counted mode, the frame size is given at the beginning
of the message. With this parameter the max size this frame can have
is specified and when the frame gets to large the mode is switched to
octet stuffing.
The max value this parameter can have was specified because otherwise
the integer could become negative and this would result in a
Segmentation Fault. (Max Value: 200000000)


MaxSessions
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Maximum number of open sessions allowed.  If more tcp connections
are created then rsyslog will drop those connections.  Warning,
this defaults to 0 which means unlimited, so take care to set this
if you have limited memory and/or processing power.

A setting of zero or negative integer means no limit.

Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$InputPTCPServerListenIP``"

On multi-homed machines, specifies to which local address the
listener should be bound.


AddtlFrameDelimiter
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "-1", "no", "``$InputPTCPServerAddtlFrameDelimiter``"

This directive permits to specify an additional frame delimiter for
plain tcp syslog. The industry-standard specifies using the LF
character as frame delimiter. Some vendors, notable Juniper in their
NetScreen products, use an invalid frame delimiter, in Juniper's case
the NUL character. This directive permits to specify the ASCII value
of the delimiter in question. Please note that this does not
guarantee that all wrong implementations can be cured with this
directive. It is not even a sure fix with all versions of NetScreen,
as I suggest the NUL character is the effect of a (common) coding
error and thus will probably go away at some time in the future. But
for the time being, the value 0 can probably be used to make rsyslog
handle NetScreen's invalid syslog/tcp framing. For additional
information, see this `forum
thread <http://kb.monitorware.com/problem-with-netscreen-log-t1652.html>`_.
**If this doesn't work for you, please do not blame the rsyslog team.
Instead file a bug report with Juniper!**

Note that a similar, but worse, issue exists with Cisco's IOS
implementation. They do not use any framing at all. This is confirmed
from Cisco's side, but there seems to be very limited interest in
fixing this issue. This directive **can not** fix the Cisco bug. That
would require much more code changes, which I was unable to do so
far. Full details can be found at the `Cisco tcp syslog
anomaly <http://www.rsyslog.com/Article321.phtml>`_ page.


SupportOctetCountedFraming
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "``$InputPTCPServerSupportOctetCountedFraming``"

The legacy octed-counted framing (similar to RFC5425
framing) is activated. This is the default and should be left
unchanged until you know very well what you do. It may be useful to
turn it off, if you know this framing is not used and some senders
emit multi-line messages into the message stream.


NotifyOnConnectionClose
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputPTCPServerNotifyOnConnectionClose``"

Instructs imptcp to emit a message if a remote peer closes the
connection.


NotifyOnConnectionOpen
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Instructs imptcp to emit a message if a remote peer opens a
connection. Hostname of the remote peer is given in the message.


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$InputPTCPServerKeepAlive``"

Enable of disable keep-alive packets at the tcp socket layer. The
default is to disable them.


KeepAlive.Probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputPTCPServerKeepAlive_probes``"

The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


KeepAlive.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputPTCPServerKeepAlive_intvl``"

The interval between subsequential keepalive probes, regardless of
what the connection has exchanged in the meantime. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


KeepAlive.Time
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$InputPTCPServerKeepAlive_time``"

The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
not be available on all platforms.


RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Specifies the rate-limiting interval in seconds. Set it to a number
of seconds (5 recommended) to activate rate-limiting.


RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10000", "no", "none"

Specifies the rate-limiting burst in number of messages.


Compression.mode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the counterpart to the compression modes set in
:doc:`omfwd <omfwd>`.
Please see it's documentation for details.


flowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Flow control is used to throttle the sender if the receiver queue is
near-full preserving some space for input that can not be throttled.


MultiLine
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Experimental parameter which causes rsyslog to recognize a new message
only if the line feed is followed by a '<' or if there are no more characters.


framing.delimiter.regex
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "off", "no", "none"

Experimental parameter. It is similar to "MultiLine", but provides greater
control of when a log message ends. You can specify a regular expression that
characterizes the header to expect at the start of the next message. As such,
it indicates the end of the current message. For example, one can use this
setting to use a RFC3164 header as frame delimiter::

    framing.delimiter.regex="^<[0-9]{1,3}>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"

Note that when oversize messages arrive this mode may have problems finding
the proper frame terminator. There are some provisions inside imptcp to make
these kinds of problems unlikely, but if the messages are very much over the
configured MaxMessageSize, imptcp emits an error messages. Chances are great
it will properly recover from such a situation.


SocketBacklog
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "5", "no", "none"

Specifies the backlog parameter sent to the listen() function.
It defines the maximum length to which the queue of pending connections may grow.
See man page of listen(2) for more information.
The parameter controls both TCP and UNIX sockets backlog parameter.
Default value is arbitrary set to 5.


Defaulttz
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Set default time zone. At most seven chars are set, as we would otherwise
overrun our buffer.


Framingfix.cisco.asa
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Cisco very occasionally sends a space after a line feed, which thrashes framing
if not taken special care of. When this parameter is set to "on", we permit
space *in front of the next frame* and ignore it.


ListenPortFileName
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.38.0

With this parameter you can specify the name for a file. In this file the
port, imptcp is connected to, will be written.
This parameter was introduced because the testbench works with dynamic ports.


.. _imptcp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener. The statistic is
named "imtcp" , followed by the bound address, listener port and IP
version in parenthesis. For example, the counter for a listener on port
514, bound to all interfaces and listening on IPv6 is called
"imptcp(\*/514/IPv6)".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


.. _error-messages:

Error Messages
==============

When a message is to long it will be truncated and an error will show the remaining length of the message and the beginning of it. It will be easier to comprehend the truncation.


Caveats/Known Bugs
==================

-  module always binds to all interfaces


Examples
========

Example 1
---------

This sets up a TCP server on port 514:

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" port="514")


Example 2
---------

This creates a listener that listens on the local loopback
interface, only.

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" port="514" address="127.0.0.1")


Example 3
---------

Create a unix domain socket:

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" path="/tmp/unix.sock" unlink="on")


