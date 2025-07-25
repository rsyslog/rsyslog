.. _omczmq:

********************************
omczmq: Output module for ZeroMQ
********************************

.. index:: ! omczmq

===========================  ===================================================
**Module Name:**             **omczmq**
**Author:**                  Brian Knox <bknox@digitalocean.com>
===========================  ===================================================

Purpose
=======

The ``omczmq`` module publishes messages over ZeroMQ using the CZMQ
library. Messages are formatted with templates and sent according to the
configured socket type and topic list. The module supports dynamic
topics, CURVE authentication and heartbeat settings when available in the
underlying libraries.

Configuration Parameters
========================

.. note::
   Parameter names are case-insensitive.

Module Parameters
-----------------

``authenticator``
  Start a ZAauth authenticator thread when set to ``on``.

``authtype``
  Set to ``CURVECLIENT`` or ``CURVESERVER`` to enable CURVE security.

``servercertpath``
  Path to the server certificate used for CURVE.

``clientcertpath``
  Path to the client certificate or directory of allowed clients.

Action Parameters
-----------------

``endpoints``
  Space-separated list of ZeroMQ endpoints to connect or bind.

``socktype``
  ZeroMQ socket type such as ``PUSH``, ``PUB``, ``DEALER``, ``RADIO`` or ``CLIENT``.

``sendtimeout``
  Timeout in milliseconds before a send operation fails.

``sendhwm``
  Maximum number of queued messages before new ones are dropped.

``connecttimeout``
  Connection timeout in milliseconds (requires libzmq 4.2 or later).

``heartbeativl``
  Interval in milliseconds between heartbeat pings (libzmq >= 4.2).

``heartbeattimeout``
  Time in milliseconds to wait for a heartbeat reply (libzmq >= 4.2).

``heartbeatttl``
  Time a peer may wait between pings before disconnecting (libzmq >= 4.2).

``template``
  Name of the template used to format messages. Defaults to ``RSYSLOG_ForwardFormat``.

``topics``
  Comma-separated list of topics for ``PUB`` or ``RADIO`` sockets.

``topicframe``
  When ``on`` the topic is sent as a separate frame on ``PUB`` sockets.

``dynatopic``
  If ``on`` each topic name is treated as a template.

Examples
========

.. code-block:: none

   module(
       load="omczmq"                          # load the output module
       servercertpath="/etc/curve.d/example_server"  # server certificate path
       clientcertpath="/etc/curve.d/allowed_clients" # allowed client directory
       authtype="CURVESERVER"                 # enable CURVE server mode
       authenticator="on"                     # start ZAauth authenticator
   )

   template(name="host_program_topic" type="list") { # build a dynamic topic
       property(name="hostname")                     # host segment
       constant(value=".")                           # separator
       property(name="programname")                  # program segment
   }

   action(
       type="omczmq"                          # use the omczmq output
       socktype="PUB"                         # create a PUB socket
       endpoints="@tcp://*:31338"             # bind to port 31338
       topics="host_program_topic"            # topic template name
       dynatopic="on"                         # treat topic as template
       topicframe="on"                        # send topic as separate frame
   )

