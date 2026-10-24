.. SPDX-License-Identifier: Apache-2.0

.. _imczmq:

*******************************
imczmq: Input module for ZeroMQ
*******************************

.. meta::
   :description: Receive ZeroMQ messages with imczmq and configure comma-separated endpoints.
   :keywords: rsyslog, imczmq, ZeroMQ, CZMQ, endpoints

.. index:: ! imczmq

===========================  ===================================================
**Module Name:**             **imczmq**
**Author:**                  Brian Knox <bknox@digitalocean.com>
===========================  ===================================================

Purpose
=======

.. summary-start

The ``imczmq`` module receives log messages from ZeroMQ sockets using the
CZMQ library. Each incoming frame is converted into an rsyslog message and
processed by the configured ruleset. Multiple socket types and optional
CURVE authentication are supported.

.. summary-end

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
  Comma-separated list of ZeroMQ endpoints to bind or connect to. Do not
  separate endpoints with spaces.

``socktype``
  ZeroMQ socket type such as ``PULL``, ``SUB``, ``ROUTER``, ``DISH`` or ``SERVER``.

``ruleset``
  Optional ruleset that processes received messages.

``topics``
  Optional comma-separated list of topics for ``SUB`` or ``DISH`` sockets.
  An empty topic subscribes to all.

Examples
========

To receive from two local IPC endpoints, separate their addresses with a comma.
The ``/run/rsyslog`` directory must exist and allow the rsyslog process to
create socket entries before the input starts. The example writes received
messages to ``zeromq.log`` in the working directory.

.. rsyslog-doc-sample: validate-config
   :require-plugin: imczmq

.. code-block:: rsyslog

   module(load="imczmq")
   input(type="imczmq" socktype="PULL"
         endpoints="ipc:///run/rsyslog/input-one.sock,ipc:///run/rsyslog/input-two.sock")
   action(type="omfile" file="./zeromq.log")


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

