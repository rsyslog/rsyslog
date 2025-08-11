.. _imczmq:

*******************************
imczmq: Input module for ZeroMQ
*******************************

.. index:: ! imczmq

===========================  ===================================================
**Module Name:**             **imczmq**
**Author:**                  Brian Knox <bknox@digitalocean.com>
===========================  ===================================================

Purpose
=======

The ``imczmq`` module receives log messages from ZeroMQ sockets using the
CZMQ library. Each incoming frame is converted into an rsyslog message and
processed by the configured ruleset. Multiple socket types and optional
CURVE authentication are supported.

Configuration Parameters
========================

.. note::
   Parameter names are case-insensitive.

Module Parameters
-----------------

``authenticator``
  Start a ZAauth authenticator thread when set to ``on``.

``authtype``
  ``CURVECLIENT`` or ``CURVESERVER`` to enable CURVE security.

``servercertpath``
  Path to the server certificate used with CURVE.

``clientcertpath``
  Path to the client certificate or directory of allowed clients.

Input Parameters
----------------

``endpoints``
  Space-separated list of ZeroMQ endpoints to bind or connect to.

``socktype``
  ZeroMQ socket type such as ``PULL``, ``SUB``, ``ROUTER``, ``DISH`` or ``SERVER``.

``ruleset``
  Optional ruleset that processes received messages.

``topics``
  Optional comma-separated list of topics for ``SUB`` or ``DISH`` sockets.
  An empty topic subscribes to all.

Examples
========

.. code-block:: none

   module(
       load="imczmq"                     # load the input module
       servercertpath="/etc/curve.d/server"  # path to server certificate
       clientcertpath="/etc/curve.d/"        # directory with client certs
       authtype="CURVESERVER"               # enable CURVE server mode
       authenticator="on"                   # start ZAauth authenticator
   )

   input(
       type="imczmq"                       # use the imczmq input
       socktype="PULL"                     # create a PULL socket
       endpoints="@tcp://*:24555"          # bind on port 24555
   )

