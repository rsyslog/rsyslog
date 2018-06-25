**************************************
omfwd: syslog Forwarding Output Module
**************************************

===========================  ===========================================================================
**Module Name:**             **omfwd**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

The omfwd plug-in provides the core functionality of traditional message
forwarding via UDP and plain TCP. It is a built-in module that does not
need to be loaded.

 
Notable Features
================


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Module Parameters
-----------------

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "``$ActionForwardDefaultTemplateName``"

Sets a non-standard default template for this module.
 

Action Parameters
-----------------

Target
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Name or IP-Address of the system that shall receive messages. Any
resolvable name is fine.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "514", "no", "none"

Name or numerical value of port to use when connecting to target.


Protocol
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "udp", "no", "none"

Type of protocol to use for forwarding. Note that \`\`tcp'' means
both legacy plain tcp syslog as well as RFC5425-based TLS-encrypted
syslog. Which one is selected depends on the StreamDriver parameter.
If StreamDriver is set to "ossl" or "gtls" it will use TLS-encrypted syslog.


NetworkNamespace
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Name of a network namespace as in /var/run/netns/ to use for forwarding.

If the setns() system call is not available on the system (e.g. BSD
kernel, linux kernel before v2.6.24) the given namespace will be
ignored.

Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.35.0

Bind socket to a given local IP address. This option is only supported
for UDP, not TCP.

IpFreeBind
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "2", "no", "none"

.. versionadded:: 8.35.0

Manages the IP_FREEBIND option on the UDP socket, which allows binding it to
an IP address that is not yet associated to any network interface. This option
is only relevant if the address option is set.

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

   "word", "none", "no", "none"

Bind socket to given device (e.g., eth0)

For Linux with VRF support, the Device option can be used to specify the
VRF for the Target address.


TCP_Framing
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "traditional", "no", "none"

Framing-Mode to be for forwarding. This affects only TCP-based
protocols. It is ignored for UDP. In protocol engineering,
"framing" means how multiple messages over the same connection
are separated. Usually, this is transparent to users. Unfortunately,
the early syslog protocol evolved, and so there are cases where users
need to specify the framing. The traditional framing is
nontransparent. With it, messages are end when a LF (aka "line
break", "return") is encountered, and the next message starts
immediately after the LF. If multi-line messages are received, these
are essentially broken up into multiple message, usually with all but
the first message segment being incorrectly formatted. The
octet-counting framing solves this issue. With it, each message is
prefixed with the actual message length, so that a receivers knows
exactly where the message ends. Multi-line messages cause no problem
here. This mode is very close to the method described in RFC5425 for
TLS-enabled syslog. Unfortunately, only few syslogd implementations
support octet-counted framing. As such, the traditional framing is
set as default, even though it has defects. If it is known that the
receiver supports octet-counted framing, it is suggested to use that
framing mode.


TCP_FrameDelimiter
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "10", "no", "none"

Sets a custom frame delimiter for TCP transmission when running TCP\_Framing
in "traditional" mode. The delimiter has to be a number between 0 and 255
(representing the ASCII-code of said character). The default value for this
parameter is 10, representing a '\\n'. When using Graylog, the parameter
must be set to 0.


ZipLevel
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

Compression level for messages.

Up until rsyslog 7.5.1, this was the only compression setting that
rsyslog understood. Starting with 7.5.1, we have different
compression modes. All of them are affected by the ziplevel. If,
however, no mode is explicitely set, setting ziplevel also turns on
"single" compression mode, so pre 7.5.1 configuration will continue
to work as expected.

The compression level is specified via the usual factor of 0 to 9,
with 9 being the strongest compression (taking up most processing
time) and 0 being no compression at all (taking up no extra
processing time).


compression.Mode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

*mode* is one of "none", "single", or "stream:always". The default
is "none", in which no compression happens at all.
In "single" compression mode, Rsyslog implements a proprietary
capability to zip transmitted messages. That compression happens on a
message-per-message basis. As such, there is a performance gain only
for larger messages. Before compressing a message, rsyslog checks if
there is some gain by compression. If so, the message is sent
compressed. If not, it is sent uncompressed. As such, it is totally
valid that compressed and uncompressed messages are intermixed within
a conversation.

In "stream:always" compression mode the full stream is being
compressed. This also uses non-standard protocol and is compatible
only with receives that have the same abilities. This mode offers
potentially very high compression ratios. With typical syslog
messages, it can be as high as 95+% compression (so only one
twentieth of data is actually transmitted!). Note that this mode
introduces extra latency, as data is only sent when the compressor
emits new compressed data. For typical syslog messages, this can mean
that some hundered messages may be held in local buffers before they
are actually sent. This mode has been introduced in 7.5.1.

**Note: currently only imptcp supports receiving stream-compressed
data.**


compression.stream.flushOnTXEnd
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

.. versionadded:: 7.5.3

This setting affects stream compression mode, only. If enabled (the
default), the compression buffer will by emptied at the end of a
rsyslog batch. If set to "off", end of batch will not affect
compression at all.

While setting it to "off" can potentially greatly improve
compression ratio, it will also introduce severe delay between when a
message is being processed by rsyslog and actually sent out to the
network. We have seen cases where for several thousand message not a
single byte was sent. This is good in the sense that it can happen
only if we have a great compression ratio. This is most probably a
very good mode for busy machines which will process several thousand
messages per second and te resulting short delay will not pose any
problems. However, the default is more conservative, while it works
more "naturally" with even low message traffic. Even in flush mode,
notable compression should be achivable (but we do not yet have
practice reports on actual compression ratios).


RebindInterval
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionSendTCPRebindInterval`` or ``$ActionSendUDPRebindInterval``"

Permits to specify an interval at which the current connection is
broken and re-established. This setting is primarily an aid to load
balancers. After the configured number of messages has been
transmitted, the current connection is terminated and a new one
started. Note that this setting applies to both TCP and UDP traffic.
For UDP, the new \`\`connection'' uses a different source port (ports
are cycled and not reused too frequently). This usually is perceived
as a \`\`new connection'' by load balancers, which in turn forward
messages to another physical target system.


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Enable or disable keep-alive packets at the tcp socket layer. The
default is to disable them.


KeepAlive.Probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

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

   "integer", "0", "no", "none"

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

   "integer", "0", "no", "none"

The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
not be available on all platforms.


StreamDriver
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionSendStreamDriver``"

Choose the stream driver to be used. Default is plain tcp, but
you can also choose "ossl" or "gtls" for TLS encryption.


StreamDriverMode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionSendStreamDriverMode``"

Mode to use with the stream driver (driver-specific)


StreamDriverAuthMode
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$ActionSendStreamDriverAuthMode``"

Authentication mode to use with the stream driver. Note that this
parameter requires TLS netstream drivers. For all others, it will be
ignored. (driver-specific).


StreamDriverPermittedPeers
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionSendStreamDriverPermittedPeers``"

Accepted fingerprint (SHA1) or name of remote peer. Note that this
parameter requires TLS netstream drivers. For all others, it will be
ignored. (driver-specific)


ResendLastMSGOnReconnect
^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "``$ActionSendResendLastMsgOnReconnect``"

Permits to resend the last message when a connection is reconnected.
This setting affects TCP-based syslog, only. It is most useful for
traditional, plain TCP syslog. Using this protocol, it is not always
possible to know which messages were successfully transmitted to the
receiver when a connection breaks. In many cases, the last message
sent is lost. By switching this setting to "yes", rsyslog will always
retransmit the last message when a connection is reestablished. This
reduces potential message loss, but comes at the price that some
messages may be duplicated (what usually is more acceptable).

Please note that busy systems probably loose more than a
single message in such cases. This is caused by an
`inherant unreliability in plain tcp syslog
<https://rainer.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html>`_
and there is no way rsyslog could prevent this from happening
(if you read the detail description, be sure to follow the link
to the follow-up posting). In order to prevent these problems,
we recommend the use of :doc:`omrelp <omrelp>`.


udp.SendToAll
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When sending UDP messages, there are potentially multiple paths to
the target destination. By default, rsyslogd
only sends to the first target it can successfully send to. If this
option is set to "on", messages are sent to all targets. This may improve
reliability, but may also cause message duplication. This option
should be enabled only if it is fully understood.

Note: this option replaces the former -A command line option. In
contrast to the -A option, this option must be set once per
input() definition.


udp.SendDelay
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.7.0

This is an **expert option**, do only use it if you know very well
why you are using it!

This options permits to introduce a small delay after *each* send
operation. The integer specifies the delay in microseconds. This
option can be used in cases where too-quick sending of UDP messages
causes message loss (UDP is permitted to drop packets if e.g. a device
runs out of buffers). Usually, you do not want this delay. The parameter
was introduced in order to support some testbench tests. Be sure
to think twice before you use it in producetion.


gnutlsPriorityString
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.29.0

The GnuTLS priority strings specify the TLS session's handshake algorithms and
options. These strings are intended as a user-specified override of the library
defaults. If this parameter is NULL, the default settings are used. More
information about priority Strings
`here <https://gnutls.org/manual/html_node/Priority-Strings.html>`_.


See Also
========

-  `Encrypted Disk
   Queues <http://www.rsyslog.com/encrypted-disk-queues/>`_


Examples
========

Example 1
---------

The following command sends all syslog messages to a remote server via
TCP port 10514.

.. code-block:: none

   action(type="omfwd" Target="192.168.2.11" Port="10514" Protocol="tcp" Device="eth0")


Example 2
---------

In case the system in use has multiple (maybe virtual) network interfaces network
namespaces come in handy, each with its own routing table. To be able to distribute
syslogs to remote servers in different namespaces specify them as separate actions.

.. code-block:: none

   action(type="omfwd" Target="192.168.1.13" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.0")
   action(type="omfwd" Target="192.168.2.24" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.1")
   action(type="omfwd" Target="192.168.3.38" Port="10514" Protocol="tcp" NetworkNamespace="ns_eth0.2")


