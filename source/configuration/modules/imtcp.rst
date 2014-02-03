`back to rsyslog module overview <rsyslog_conf_modules.html>`_

TCP Syslog Input Module
=======================

**Module Name:    imtcp**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Multi-Ruleset Support:**\ since 4.5.0 and 5.1.1

**Description**:

Provides the ability to receive syslog messages via TCP. Encryption is
natively provided by selecting the approprioate network stream driver
and can also be provided by using `stunnel <rsyslog_stunnel.html>`_ (an
alternative is the use the `imgssapi <imgssapi.html>`_ module).

**Configuration Directives**:

**Global Directives**:

-  **AddtlFrameDelimiter** <Delimiter>
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
-  **DisableLFDelimiter** <on/**off**>
    Industry-strandard plain text tcp syslog uses the LF to delimit
   syslog frames. However, some users brought up the case that it may be
   useful to define a different delimiter and totally disable LF as a
   delimiter (the use case named were multi-line messages). This mode is
   non-standard and will probably come with a lot of problems. However,
   as there is need for it and it is relatively easy to support, we do
   so. Be sure to turn this setting to "on" only if you exactly know
   what you are doing. You may run into all sorts of troubles, so be
   prepared to wrangle with that!
-  **NotifyOnConnectionClose** [on/**off**]
    instructs imtcp to emit a message if the remote peer closes a
   connection.
    **Important:** This directive is global to all listeners and must be
   given right after loading imtcp, otherwise it may have no effect.
-  **KeepAlive** <on/**off**>
    enable of disable keep-alive packets at the tcp socket layer. The
   default is to disable them.
-  **FlowControl** <**on**/off>
    This setting specifies whether some message flow control shall be
   exercised on the related TCP input. If set to on, messages are
   handled as "light delayable", which means the sender is throttled a
   bit when the queue becomes near-full. This is done in order to
   preserve some queue space for inputs that can not throttle (like
   UDP), but it may have some undesired effect in some configurations.
   Still, we consider this as a useful setting and thus it is the
   default. To turn the handling off, simply configure that explicitely.
-  **MaxListeners** <number>
    Sets the maximum number of listeners (server ports) supported.
   Default is 20. This must be set before the first $InputTCPServerRun
   directive.
-  **MaxSessions** <number>
    Sets the maximum number of sessions supported. Default is 200. This
   must be set before the first $InputTCPServerRun directive
-  **StreamDriver.Mode** <number>
    Sets the driver mode for the currently selected `network stream
   driver <netstream.html>`_. <number> is driver specifc.
-  **StreamDriver.AuthMode** <mode-string>
    Sets the authentication mode for the currently selected `network
   stream driver <netstream.html>`_. <mode-string> is driver specifc.
-  **PermittedPeer** <id-string>
    Sets permitted peer IDs. Only these peers are able to connect to the
   listener. <id-string> semantics depend on the currently selected
   AuthMode and  `network stream driver <netstream.html>`_.
   PermittedPeer may not be set in anonymous modes.
   PermittedPeer may be set either to a single peer or an array of peers
   either of type IP or name, depending on the tls certificate.
   Single peer: PermittedPeer="127.0.0.1"
   Array of peers:
   PermittedPeer=["test1.example.net","10.1.2.3","test2.example.net","..."]

**Action Directives**:

-  **Port** <port>
    Starts a TCP server on selected port
-  **Name** <name>
    Sets a name for the inputname property. If no name is set "imtcp" is
   used by default. Setting a name is not strictly necessary, but can be
   useful to apply filtering based on which input the message was
   received from.
-  **Ruleset** <ruleset>
    Binds the listener to a specific `ruleset <multi_ruleset.html>`_.
-  **SupportOctetCountedFraming** <**on**\ \|off>
    If set to "on", the legacy octed-counted framing (similar to RFC5425
   framing) is activated. This is the default and should be left
   unchanged until you know very well what you do. It may be useful to
   turn it off, if you know this framing is not used and some senders
   emit multi-line messages into the message stream.
-  **RateLimit.Interval** [number] - (available since 7.3.1) specifies
   the rate-limiting interval in seconds. Default value is 0, which
   turns off rate limiting. Set it to a number of seconds (5
   recommended) to activate rate-limiting.
-  **RateLimit.Burst** [number] - (available since 7.3.1) specifies the
   rate-limiting burst in number of messages. Default is 10,000.

**Caveats/Known Bugs:**

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)

**Example:**

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

module(load="imtcp" MaxSessions="500") input(type="imtcp" port="514")

Note that the global parameters (here: max sessions) need to be set when
the module is loaded. Otherwise, the parameters will not apply.

**Legacy Configuration Directives**:

-  **$InputTCPServerAddtlFrameDelimiter <Delimiter>**
    equivalent to: AddtlFrameDelimiter
-  **$InputTCPServerDisableLFDelimiter** <on/**off**> (available since
   5.5.3)
    equivalent to: DisableLFDelimiter
-  **$InputTCPServerNotifyOnConnectionClose** [on/**off**] (available
   since 4.5.5)
    equivalent to: NotifyOnConnectionClose
-  **$InputTCPServerKeepAlive** <on/**off**>
    equivalent to: KeepAlive
-  **$InputTCPServerRun** <port>
    equivalent to: Port
-  **$InputTCPFlowControl** <**on**/off>
    equivalent to: FlowControl
-  **$InputTCPMaxListeners** <number>
    equivalent to: MaxListeners
-  **$InputTCPMaxSessions** <number>
    equivalent to: MaxSessions
-  **$InputTCPServerStreamDriverMode** <number>
    equivalent to: StreamDriver.Mode
-  **$InputTCPServerInputName** <name>
    equivalent to: Name
-  **$InputTCPServerStreamDriverAuthMode** <mode-string>
    equivalent to: StreamDriver.AuthMode
-  **$InputTCPServerStreamDriverPermittedPeer** <id-string>
    equivalent to: PermittedPeer.
-  **$InputTCPServerBindRuleset** <ruleset>
    equivalent to: Ruleset.
-  **$InputTCPSupportOctetCountedFraming** <**on**\ \|off>
    equivalent to: SupportOctetCountedFraming

**Caveats/Known Bugs:**

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)

**Example:**

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

$ModLoad imtcp # needs to be done just once $InputTCPMaxSessions 500
$InputTCPServerRun 514

Note that the parameters (here: max sessions) need to be set **before**
the listener is activated. Otherwise, the parameters will not apply.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008,2009 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
