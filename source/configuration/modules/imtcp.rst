imtcp: TCP Syslog Input Module
==============================

Provides the ability to receive syslog messages via TCP. Encryption is
natively provided by selecting the approprioate network stream driver
and can also be provided by using `stunnel <rsyslog_stunnel.html>`_ (an
alternative is the use the `imgssapi <imgssapi.html>`_ module).

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

Configuration Parameters
------------------------

Global Parameters
^^^^^^^^^^^^^^^^^

.. function:: AddtlFrameDelimiter <Delimiter>

   This directive permits to specify an additional frame delimiter for
   Multiple receivers may be configured by specifying $InputTCPServerRun
   multiple times. This is available since version 4.3.1, earlier 
   versions do NOT support it.

.. function:: DisableLFDelimiter on/off

   *Default is off*

   Industry-strandard plain text tcp syslog uses the LF to delimit
   syslog frames. However, some users brought up the case that it may be
   useful to define a different delimiter and totally disable LF as a
   delimiter (the use case named were multi-line messages). This mode is
   non-standard and will probably come with a lot of problems. However,
   as there is need for it and it is relatively easy to support, we do
   so. Be sure to turn this setting to "on" only if you exactly know
   what you are doing. You may run into all sorts of troubles, so be
   prepared to wrangle with that!

.. function:: NotifyOnConnectionClose on/off

   *Default is off*

   Instructs imtcp to emit a message if the remote peer closes a
   connection.

   **Important:** This directive is global to all listeners and must be
   given right after loading imtcp, otherwise it may have no effect.

.. function:: KeepAlive on/off

   *Default is off*

    enable of disable keep-alive packets at the tcp socket layer. The
   default is to disable them.

.. function:: FlowControl on/off

   *Default is on*

   This setting specifies whether some message flow control shall be
   exercised on the related TCP input. If set to on, messages are
   handled as "light delayable", which means the sender is throttled a
   bit when the queue becomes near-full. This is done in order to
   preserve some queue space for inputs that can not throttle (like
   UDP), but it may have some undesired effect in some configurations.
   Still, we consider this as a useful setting and thus it is the
   default. To turn the handling off, simply configure that explicitely.

.. function:: MaxListeners <number>

   *Default is 20*

   Sets the maximum number of listeners (server ports) supported.
   This must be set before the first $InputTCPServerRun directive.

.. function:: MaxSessions <number>

   *Default is 200*

   Sets the maximum number of sessions supported. This must be set 
   before the first $InputTCPServerRun directive

.. function:: StreamDriver.Mode <number>

   Sets the driver mode for the currently selected `network stream
   driver <netstream.html>`_. <number> is driver specific.

.. function:: StreamDriver.AuthMode <mode-string>

   Sets permitted peer IDs. Only these peers are able to connect to the
   listener. <id-string> semantics depend on the currently selected
   AuthMode and  `network stream driver <netstream.html>`_.
   PermittedPeers may not be set in anonymous modes.

.. function:: PermittedPeer <id-string>

   Sets permitted peer IDs. Only these peers are able to connect to the
   listener. <id-string> semantics depend on the currently selected
   AuthMode and  `network stream driver <netstream.html>`_.
   PermittedPeer may not be set in anonymous modes.
   PermittedPeer may be set either to a single peer or an array of peers
   either of type IP or name, depending on the tls certificate.

   Single peer: PermittedPeer="127.0.0.1"

   Array of peers:
   PermittedPeer=["test1.example.net","10.1.2.3","test2.example.net","..."]

Action Parameters
^^^^^^^^^^^^^^^^^

.. function:: Port <port>

   Starts a TCP server on selected port

.. function:: Name <name>
   Sets a name for the inputname property. If no name is set "imtcp" is
   used by default. Setting a name is not strictly necessary, but can be
   useful to apply filtering based on which input the message was
   received from.

.. function:: Ruleset <ruleset>
   
   Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

.. function:: SupportOctetCountedFraming on/off

   *Default is on*

   If set to "on", the legacy octed-counted framing (similar to RFC5425
   framing) is activated. This should be left unchanged until you know 
   very well what you do. It may be useful to turn it off, if you know 
   this framing is not used and some senders emit multi-line messages 
   into the message stream.

.. function:: RateLimit.Interval [number]
   Specifies the rate-limiting interval in seconds. Default value is 0, 
   which turns off rate limiting. Set it to a number of seconds (5
   recommended) to activate rate-limiting.

.. function:: RateLimit.Burst [number]
   Specifies the rate-limiting burst in number of messages. Default is 
   10,000. 

Caveats/Known Bugs
------------------

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)

Example
-------

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

::

  module(load="imtcp" MaxSessions="500")
  input(type="imtcp" port="514")

Note that the global parameters (here: max sessions) need to be set when
the module is loaded. Otherwise, the parameters will not apply.

Legacy Configuration Directives
-------------------------------

.. function:: $InputTCPServerAddtlFrameDelimiter <Delimiter>
   equivalent to: AddtlFrameDelimiter
.. function:: $InputTCPServerDisableLFDelimiter on/off
   equivalent to: DisableLFDelimiter
.. function:: $InputTCPServerNotifyOnConnectionClose on/off
   equivalent to: NotifyOnConnectionClose
.. function:: $InputTCPServerKeepAlive** <on/**off**>
   equivalent to: KeepAlive
.. function:: $InputTCPServerRun <port>
   equivalent to: Port
.. function:: $InputTCPFlowControl on/off
   equivalent to: FlowControl
.. function:: $InputTCPMaxListeners <number>
   equivalent to: MaxListeners
.. function:: $InputTCPMaxSessions <number>
   equivalent to: MaxSessions
.. function:: $InputTCPServerStreamDriverMode <number>
   equivalent to: StreamDriver.Mode
.. function:: $InputTCPServerInputName <name>
   equivalent to: Name
.. function:: $InputTCPServerStreamDriverAuthMode <mode-string>
   equivalent to: StreamDriver.AuthMode
.. function:: $InputTCPServerStreamDriverPermittedPeer <id-string>
   equivalent to: PermittedPeer.
.. function:: $InputTCPServerBindRuleset <ruleset>
   equivalent to: Ruleset.
.. function:: $InputTCPSupportOctetCountedFraming on/off
   equivalent to: SupportOctetCountedFraming

Caveats/Known Bugs
------------------

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)

Example
-------

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

::

  $ModLoad imtcp # needs to be done just once
  $InputTCPMaxSessions 500
  $InputTCPServerRun 514

Note that the parameters (here: max sessions) need to be set **before**
the listener is activated. Otherwise, the parameters will not apply.

