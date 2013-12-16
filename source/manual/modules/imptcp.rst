`back <rsyslog_conf_modules.html>`_

Plain TCP Syslog Input Module
=============================

**Module Name:    imptcp**

**Available since:**\ 4.7.3+, 5.5.8+

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

Provides the ability to receive syslog messages via plain TCP syslog.
This is a specialised input plugin tailored for high performance on
Linux. It will probably not run on any other platform. Also, it does no
provide TLS services. Encryption can be provided by using
`stunnel <rsyslog_stunnel.html>`_.

This module has no limit on the number of listeners and sessions that
can be used.

Multiple receivers may be configured by specifying $InputPTCPServerRun
multiple times.

**Configuration Directives**:

This plugin has config directives similar named as imtcp, but they all
have **P**\ TCP in their name instead of just TCP. Note that only a
subset of the parameters are supported.

-  $InputPTCPServerAddtlFrameDelimiter <Delimiter>
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
-  **$InputPTCPSupportOctetCountedFraming** <**on**\ \|off>
    If set to "on", the legacy octed-counted framing (similar to RFC5425
   framing) is activated. This is the default and should be left
   unchanged until you know very well what you do. It may be useful to
   turn it off, if you know this framing is not used and some senders
   emit multi-line messages into the message stream.
-  $InputPTCPServerNotifyOnConnectionClose [on/**off**]
    instructs imptcp to emit a message if the remote peer closes a
   connection.
-  **$InputPTCPServerKeepAlive** <on/**off**>
    enable of disable keep-alive packets at the tcp socket layer. The
   default is to disable them.
-  **$InputPTCPServerKeepAlive\_probes** <number>
    The number of unacknowledged probes to send before considering the
   connection dead and notifying the application layer. The default, 0,
   means that the operating system defaults are used. This has only
   effect if keep-alive is enabled. The functionality may not be
   available on all platforms.
-  **$InputPTCPServerKeepAlive\_intvl** <number>
    The interval between subsequential keepalive probes, regardless of
   what the connection has exchanged in the meantime. The default, 0,
   means that the operating system defaults are used. This has only
   effect if keep-alive is enabled. The functionality may not be
   available on all platforms.
-  **$InputPTCPServerKeepAlive\_time** <number>
    The interval between the last data packet sent (simple ACKs are not
   considered data) and the first keepalive probe; after the connection
   is marked to need keepalive, this counter is not used any further.
   The default, 0, means that the operating system defaults are used.
   This has only effect if keep-alive is enabled. The functionality may
   not be available on all platforms.
-  **$InputPTCPServerRun** <port>
    Starts a TCP server on selected port
-  $InputPTCPServerInputName <name>
    Sets a name for the inputname property. If no name is set "imptcp"
   is used by default. Setting a name is not strictly necessary, but can
   be useful to apply filtering based on which input the message was
   received from.
-  $InputPTCPServerBindRuleset <name>
    Binds specified ruleset to next server defined.
-  $InputPTCPServerListenIP <name>
    On multi-homed machines, specifies to which local address the next
   listerner should be bound.

**Caveats/Known Bugs:**

-  module always binds to all interfaces

**Sample:**

This sets up a TCP server on port 514:

$ModLoad imptcp # needs to be done just once $InputPTCPServerRun 514

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
