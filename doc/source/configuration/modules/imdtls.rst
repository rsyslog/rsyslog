**********************************************************
imdtls: Input Module for DTLS Protocol over UDP
**********************************************************

===========================  ===========================================================================
**Module Name:**             **imdtls**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
**Available since:**         v8.2402.0
===========================  ===========================================================================


Purpose
=======

The imdtls module for rsyslog is designed to securely transport syslog messages over the network using
the Datagram Transport Layer Security (DTLS) protocol. This module leverages the robustness and
security features of OpenSSL to provide an encrypted transport mechanism for syslog messages via UDP.

DTLS, being an adaptation of TLS for datagram-based protocols, offers integrity, authenticity, and
confidentiality for messages in transit. The imdtls module is particularly useful in environments
where secure transmission of log data is crucial, such as in compliance-driven industries or when
transmitting across untrusted networks.

By operating over UDP, imdtls offers the benefits of lower latency and reduced protocol overhead
compared to TCP-based transport, making it well-suited for high-throughput logging scenarios or in
networks where connection-oriented protocols may face challenges.


Requirements
============

To receive messages by DTLS you will need to fulfill the following requirements:

-  OpenSSL 1.0.2 or Higher


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

Module Parameters
-----------------

This module currently has no module-level parameters.

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imdtls-address`
     - .. include:: ../../reference/parameters/imdtls-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-port`
     - .. include:: ../../reference/parameters/imdtls-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-timeout`
     - .. include:: ../../reference/parameters/imdtls-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-name`
     - .. include:: ../../reference/parameters/imdtls-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-ruleset`
     - .. include:: ../../reference/parameters/imdtls-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-authmode`
     - .. include:: ../../reference/parameters/imdtls-tls-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-cacert`
     - .. include:: ../../reference/parameters/imdtls-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-mycert`
     - .. include:: ../../reference/parameters/imdtls-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-myprivkey`
     - .. include:: ../../reference/parameters/imdtls-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-tlscfgcmd`
     - .. include:: ../../reference/parameters/imdtls-tls-tlscfgcmd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imdtls-tls-permittedpeer`
     - .. include:: ../../reference/parameters/imdtls-tls-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imdtls-address
   ../../reference/parameters/imdtls-port
   ../../reference/parameters/imdtls-timeout
   ../../reference/parameters/imdtls-name
   ../../reference/parameters/imdtls-ruleset
   ../../reference/parameters/imdtls-tls-authmode
   ../../reference/parameters/imdtls-tls-cacert
   ../../reference/parameters/imdtls-tls-mycert
   ../../reference/parameters/imdtls-tls-myprivkey
   ../../reference/parameters/imdtls-tls-tlscfgcmd
   ../../reference/parameters/imdtls-tls-permittedpeer


.. _statistics-counter_imdtls_label:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for imdtls that
accumulate all action instances. The statistic origin is named "imdtls" with following counters:


- **submitted** - This counter tracks the number of log messages that have been received by the current
  input instance.

These statistics counters are updated in real-time by the rsyslog output module as log data is processed,
and they provide valuable information about the performance and operation of the input module.

For multiple actions using statistics callback, there will be one record for each action.

.. _imdtls-examples-label:

Examples
========

Example 1: Basic
----------------

The following sample does the following:

-  loads the imdtls module
-  outputs all logs to File

.. code-block:: none

   module(load="imdtls")
   input(type="imdtls" port="4433")

   action( type="omfile" file="/var/log/dtls.log")


Example 2: Require valid certificate
------------------------------------

The following sample does the following:

-  loads the imdtls module
-  Validates the client certificate, requires same CA for client and server certificate
-  outputs all logs to File

.. code-block:: none

   module(load="imdtls")
   input(type="imdtls"
         port="4433"
         tls.cacert="/etc/private/ca.pem"
         tls.mycert="/etc/private/cert.pem"
         tls.myprivkey="/etc/private/key.pem"
         tls.authmode="certvalid" )

   action( type="omfile" file="/var/log/dtls.log")

