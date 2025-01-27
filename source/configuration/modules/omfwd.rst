**************************************
omfwd: syslog Forwarding Output Module
**************************************

===========================  ===========================================================================
**Module Name:**             **omfwd**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

The `omfwd` plugin provides core functionality for traditional message forwarding 
via UDP and TCP (including TLS). This built-in module does not require loading.

.. note:: The RELP protocol is not supported by `omfwd`. Use :doc:`omrelp <omrelp>` 
   to forward messages via RELP.

 
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

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "``$ActionForwardDefaultTemplate``"

Sets a custom default template for this module.

iobuffer.maxSize
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "full size", "no", "none"

The iobuffer.maxSize parameter sets the maximum size of the I/O buffer
used by rsyslog when submitting messages to the TCP send API. This
parameter allows limiting the buffer size to a specific value and is
primarily intended for testing purposes, such as within an automated
testbench. By default, the full size of the I/O buffer is used, which
depends on the rsyslog version. If the specified size is too large, an
error is emitted, and rsyslog reverts to using the full size.

.. note::
    The I/O buffer has a fixed upper size limit for performance reasons. This limitation
    allows saving one ``malloc()`` call and indirect addressing. Therefore, the ``iobuffer.maxSize``
    parameter cannot be set to a value higher than this fixed limit.

.. note::
    This parameter should usually not be used in production environments.

Example
.......

.. code-block:: none

  module(load="omfwd" iobuffer.maxSize="8")

In this example, a very small buffer size is used. This setting helps
force rsyslog to execute code paths that are rarely used in normal
operations. It allows testing edge cases that typically cannot be
tested automatically.


Action Parameters
-----------------

Target
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array/word", "none", "no", "none"

Name or IP address of the system to receive messages. Any resolvable name is fine.
Here either a single target or an array of targets can be provided.

If an array is provided, rsyslog forms a "target pool". Inside the pool, it
performs equal load-balancing among them. Targets are changed for
each message being sent. If targets become unreachable, they will temporarily not
participate in load balancing. If all targets become offline (then and only then)
the action itself is suspended. Unreachable targets are automatically retried
by omfwd.

NOTE: target pools are ONLY available for TCP transport. If UDP is selected, an
error message is emitted and only the first target used.

Single target: Target="syslog.example.net"

Array of targets: Target=["syslog1.example.net", "syslog2.example.net", "syslog3.example.net"]

Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array/word", "514", "no", "none"

Name or numerical value of the port to use when connecting to the target.
If multiple targets are defined, different ports can be defined for each target.
To do so, use array mode. The first port will be used for the first target, the
second for the second target and so on. If fewer ports than targets are defined,
the remaining targets will use the first port configured. This also means that you
also need to define a single port, if all targets should use the same port.

Note: if more ports than targets are defined, the remaining ports are ignored and
an error message is emitted.


pool.resumeinterval
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "30 seconds", "no", "none"

If a target pool exists, "pool.resumeinterval" configures how often an unavailable
target is tried to be activated. A new connection request will be made in roughly
"pool.resumeinterval" seconds until connection is reestablished or the action become
completely suspenden (in which case the action settings take control).

Please note the word "roughly": the interval may be some seconds earlier or later
on a try-by-try basis because of other ongoing activity inside rsyslog.

Warning: we do NOT recommend to set this interval below 10 seconds, as it can lead
DoS-like reconnection behaviour. Actually, the default of 30 seconds is quite short
and should be extended if the use case permits.

Protocol
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "udp", "no", "none"

Type of protocol to use for forwarding. Note that ``tcp`` includes both legacy 
plain TCP syslog and 
`RFC5425 <https://datatracker.ietf.org/doc/html/rfc5425>`_-based TLS-encrypted 
syslog. The selection depends on the StreamDriver parameter. If StreamDriver is 
set to "ossl" or "gtls", it will use TLS-encrypted syslog.


NetworkNamespace
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Name of a network namespace in /var/run/netns/ to use for forwarding.

If the setns() system call is unavailable (e.g., BSD kernel, Linux kernel 
before v2.6.24), the given namespace will be ignored.


Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.35.0

Bind socket to a specific local IP address. This option is supported for 
UDP only, not TCP.


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

Framing mode used for forwarding: either "traditional" or "octet-counted". This 
applies only to TCP-based protocols and is ignored for UDP. In protocol 
engineering, "framing" refers to how multiple messages over the same connection 
are separated. Usually, this is transparent to users. However, the early syslog 
protocol evolved in such a way that users sometimes need to specify the framing.

"Traditional" framing is non-transparent, where messages end when an LF 
(line feed) is encountered, and the next message starts immediately after the 
LF. If multi-line messages are received, they are split into multiple messages, 
with all but the first segment usually incorrectly formatted.

"Octet-counted" framing addresses this issue. Each message is prefixed with its 
length, so the receiver knows exactly where the message ends. Multi-line 
messages are handled correctly. This mode is similar to the method described in 
`RFC5425 <https://datatracker.ietf.org/doc/html/rfc5425>`_ for TLS-enabled 
syslog. Unfortunately, few syslog implementations support "octet-counted" 
framing. As such, "traditional" framing is the default, despite its defects. 
If the receiver supports "octet-counted" framing, it is recommended to use 
that mode.


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
however, no mode is explicitly set, setting ziplevel also turns on
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
that some hundred messages may be held in local buffers before they
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
messages per second and the resulting short delay will not pose any
problems. However, the default is more conservative, while it works
more "naturally" with even low message traffic. Even in flush mode,
notable compression should be achievable (but we do not yet have
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
balancers. After the configured number of batches (equals roughly to
messages for UDP traffic, dependent on batch size for TCP) has been
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

ConErrSkip
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The ConErrSkip can be used to limit the number of network errors
recorded in logs. For example, value 10 means that each 10th error
message is logged. Note that this options should be used as the last
resort since the necessity of its use indicates network issues.
The default behavior is that all network errors are logged.

RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "", "no", "none"

Specifies the rate-limiting interval in seconds. Default value is 0,
which turns off rate limiting.

RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "none"
   :widths: auto
   :class: parameter-table

   "integer", "200", "(2^32)-1", "no", "none"

Specifies the rate-limiting burst in number of messages.


StreamDriver
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$ActionSendStreamDriver``"

The recommended alias, compatible with imtcp, is "StreamDriver.Name".

Choose the stream driver to be used. Default is plain tcp, but
you can also choose "ossl" or "gtls" for TLS encryption.

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriverMode
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "``$ActionSendStreamDriverMode``"

The recommended alias, compatible with imtcp, is "StreamDriver.Mode".

Mode to use with the stream driver (driver-specific)

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriverAuthMode
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "``$ActionSendStreamDriverAuthMode``"

The recommended alias, compatible with imtcp, is "StreamDriver.AuthMode".

Authentication mode to use with the stream driver. Note that this
parameter requires TLS netstream drivers. For all others, it will be
ignored. (driver-specific).

Note: aliases help, but are not a great solution. They may
cause confusion if both names are used together in a single
config. So care must be taken when using an alias.


StreamDriver.PermitExpiredCerts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "warn", "no", "none"

Controls how expired certificates will be handled when stream driver is in TLS mode.
It can have one of the following values:

-  on = Expired certificates are allowed

-  off = Expired certificates are not allowed  (Default, changed from warn to off since Version 8.2012.0)

-  warn = Expired certificates are allowed but warning will be logged


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


StreamDriver.CheckExtendedKeyPurpose
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Whether to check also purpose value in extended fields part of certificate
for compatibility with rsyslog operation. (driver-specific)


StreamDriver.PrioritizeSAN
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Whether to use stricter SAN/CN matching. (driver-specific)


StreamDriver.TlsVerifyDepth
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "TLS library default", "no", "none"


Specifies the allowed maximum depth for the certificate chain verification.
Support added in v8.2001.0, supported by GTLS and OpenSSL driver.
If not set, the API default will be used.
For OpenSSL, the default is 100 - see the doc for more:
https://docs.openssl.org/1.1.1/man3/SSL_CTX_set_verify/
For GnuTLS, the default is 5 - see the doc for more:
https://www.gnutls.org/manual/gnutls.html

StreamDriver.CAFile
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.CRLFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "optional", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2308.0

This permits to override the CRL (Certificate revocation list) file set via `global()` config
object at the per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.KeyFile
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.

StreamDriver.CertFile
^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "global() default", "no", "none"

.. versionadded:: 8.2108.0

This permits to override the CA file set via `global()` config object at the
per-action basis. This parameter is ignored if the netstream driver and/or its
mode does not need or support certificates.


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
to think twice before you use it in production.


gnutlsPriorityString
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.29.0

This strings setting is used to configure driver specific properties.
Historically, the setting was only meant for gnutls driver. However
with version v8.1905.0 and higher, the setting can also be used to set openssl configuration commands.

For GNUTls, the setting specifies the TLS session's handshake algorithms and
options. These strings are intended as a user-specified override of the library
defaults. If this parameter is NULL, the default settings are used. More
information about priority Strings
`here <https://gnutls.org/manual/html_node/Priority-Strings.html>`_.

For OpenSSL, the setting can be used to pass configuration commands to openssl library.
OpenSSL Version 1.0.2 or higher is required for this feature.
A list of possible commands and their valid values can be found in the documentation:
https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/

The setting can be single or multiline, each configuration command is separated by linefeed (\n).
Command and value are separated by equal sign (=). Here are a few samples:

Example 1
---------

This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3"


Example 2
---------

This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"



extendedConnectionCheck
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "true", "no", "none"

This setting permits to control if rsyslog should try to detect if the remote
syslog server has broken the current TCP connection. It is has no meaning when
UDP protocol is used.

Generally, broken connections are not easily detectable. That setting does additional
API calls to check for them. This causes some extra overhead, but is traditionally
enabled.

Especially in very busy systems it is probably worth to disable it. The extra overhead
is unlikely to bring real benefits in such scenarios.

Note: If you need reliable delivery, do NOT use plain TCP syslog transport.
Use RELP instead.


Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each forwarding action.
The statistic is named "target-port-protocol" where "target", "port", and
"protocol" are the respective configuration parameters. So an actual name might be
"192.0.2.1-514-TCP" or "example.net-10514-UDP".

The following properties are maintained for each action:

-  **bytes.sent** - total number of bytes sent to the network

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
