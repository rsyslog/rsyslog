omfwd: syslog Forwarding Output Module
======================================

**Module Name:**  **omfwd**

**Author:**       Rainer Gerhards <rgerhards@adiscon.com>

The omfwd plug-in provides the core functionality of traditional message
forwarding via UDP and plain TCP. It is a built-in module that does not
need to be loaded.

 
**Note: this documentation describes features present in v7+ of
rsyslog. If you use an older version, scroll down to "legacy
parameters".** If you prefer, you can also `obtain a specific version
of the rsyslog
documentation <http://www.rsyslog.com/how-to-obtain-a-specific-doc-version/>`_.

 

Module Parameters
-----------------

-  **Template** [templateName]

   sets a non-standard default template for this module.
 

Action Parameters
-----------------

-  **Target** string

   Name or IP-Address of the system that shall receive messages. Any
   resolvable name is fine.

-  **Port** [Default 514]

   Name or numerical value of port to use when connecting to target.

-  **Protocol** udp/tcp [default udp]

   Type of protocol to use for forwarding. Note that \`\`tcp'' means
   both legacy plain tcp syslog as well as RFC5425-based TCL-encrypted
   syslog. Which one is selected depends on the protocol drivers set
   before the action commend. Note that as of 6.3.6, there is no way to
   specify this within the action itself.

-  **TCP\_Framing** "traditional" or "octet-counted" [default traditional]

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

-  **ZipLevel** 0..9 [default 0]

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
-  **maxErrorMessages** integer [default 5], available since 7.5.4

   This sets the maximum number of error messages that omfwd emits
   during regular operations. The reason for such an upper limit is that
   error messages are conveyed back to rsyslog's input message stream.
   So if there would be no limit, an endless loop could be initiated if
   the failing action would need to process its own error messages and
   the emit a new one. This is also the reason why the default is very
   conservatively low. Note that version prior to 7.5.4 did not report
   any error messages for the same reason. Also note that with the
   initial implementation only errors during UDP forwarding are logged.

-  **compression.mode** *mode*

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

-  **compression.stream.flushOnTXEnd** *[**on**/off*] (requires 7.5.3+)

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

-  **RebindInterval** integer

   Permits to specify an interval at which the current connection is
   broken and re-established. This setting is primarily an aid to load
   balancers. After the configured number of messages has been
   transmitted, the current connection is terminated and a new one
   started. Note that this setting applies to both TCP and UDP traffic.
   For UDP, the new \`\`connection'' uses a different source port (ports
   are cycled and not reused too frequently). This usually is perceived
   as a \`\`new connection'' by load balancers, which in turn forward
   messages to another physical target system.

-  **StreamDriver** string

   Set the file owner for directories newly created. Please note that
   this setting does not affect the owner of directories already
   existing. The parameter is a user name, for which the userid is
   obtained by rsyslogd during startup processing. Interim changes to
   the user mapping are not detected.

-  **StreamDriverMode** integer [default 0]

   mode to use with the stream driver (driver-specific)

-  **StreamDriverAuthMode** string

   authentication mode to use with the stream driver. Note that this
   directive requires TLS netstream drivers. For all others, it will be
   ignored. (driver-specific).

-  **StreamDriverPermittedPeers** string

   accepted fingerprint (SHA1) or name of remote peer. Note that this
   directive requires TLS netstream drivers. For all others, it will be
   ignored. (driver-specific)

-  **ResendLastMSGOnReconnect** on/off

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
   <http://blog.gerhards.net/2008/04/on-unreliability-of-plain-tcp-syslog.html>`_
   and there is no way rsyslog could prevent this from happening
   (if you read the detail description, be sure to follow the link
   to the follow-up posting). In order to prevent these problems,
   we recommend the use of :doc:`omrelp <omrelp>`.

-  **udp.sendToAll** Boolean [on/off]

   **Default:** off

   When sending UDP messages, there are potentially multiple paths to
   the target destination. By default, rsyslogd
   only sends to the first target it can successfully send to. If this
   option is set to "on", messages are sent to all targets. This may improve
   reliability, but may also cause message duplication. This option
   should be enabled only if it is fully understood.

   Note: this option replaces the former -A command line option. In
   contrast to the -A option, this option must be set once per
   input() definition.

See Also
--------

-  `Encrypted Disk
   Queues <http://www.rsyslog.com/encrypted-disk-queues/>`_

Caveats/Known Bugs
------------------

Currently none.

Sample
------

The following command sends all syslog messages to a remote server via
TCP port 10514.

::

  action(type="omfwd" Target="192.168.2.11" Port="10514" Protocol="tcp" )

Legacy Configuration Directives
-------------------------------

-  **$ActionForwardDefaultTemplateName**\ string [templatename]
   sets a new default template for UDP and plain TCP forwarding action
-  **$ActionSendTCPRebindInterval**\ integer
   instructs the TCP send action to close and re-open the connection to
   the remote host every nbr of messages sent. Zero, the default, means
   that no such processing is done. This directive is useful for use
   with load-balancers. Note that there is some performance overhead
   associated with it, so it is advisable to not too often "rebind" the
   connection (what "too often" actually means depends on your
   configuration, a rule of thumb is that it should be not be much more
   often than once per second).
-  **$ActionSendUDPRebindInterval**\ integer
   instructs the UDP send action to rebind the send socket every nbr of
   messages sent. Zero, the default, means that no rebind is done. This
   directive is useful for use with load-balancers.
-  **$ActionSendStreamDriver**\ <driver basename>
   just like $DefaultNetstreamDriver, but for the specific action
-  **$ActionSendStreamDriverMode**\ <mode> [default 0]
   mode to use with the stream driver (driver-specific)
-  **$ActionSendStreamDriverAuthMode**\ <mode>
   authentication mode to use with the stream driver. Note that this
   directive requires TLS netstream drivers. For all others, it will be
   ignored. (driver-specific))
-  **$ActionSendStreamDriverPermittedPeers**\ <ID>
   accepted fingerprint (SHA1) or name of remote peer. Note that this
   directive requires TLS netstream drivers. For all others, it will be
   ignored. (driver-specific)
-  **$ActionSendResendLastMsgOnReconnect**\ on/off [default off]
   specifies if the last message is to be resend when a connecition
   breaks and has been reconnected. May increase reliability, but comes
   at the risk of message duplication.
-  **$ResetConfigVariables**
   Resets all configuration variables to their default value. Any
   settings made will not be applied to configuration lines following
   the $ResetConfigVariables. This is a good method to make sure no
   side-effects exists from previous directives. This directive has no
   parameters.

Legacy Sample
-------------

The following command sends all syslog messages to a remote server via
TCP port 10514.

::

  $ModLoad omfwd
  *.* @@192.168.2.11:10514

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
