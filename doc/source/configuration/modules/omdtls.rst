**********************************************************
omdtls: Output Module for DTLS Protocol over UDP
**********************************************************

===========================  ===========================================================================
**Module Name:**             **omdtls**
**Author:**                  Andre Lorbach <alorbach@adiscon.com>
**Available since:**         v8.2402.0
===========================  ===========================================================================


Purpose
=======

The omdtls module for rsyslog is designed to securely transmit syslog data over a network using 
the Datagram Transport Layer Security (DTLS) protocol. This module leverages the robustness and
security features of OpenSSL to provide an encrypted transport mechanism for syslog messages via UDP.

DTLS, being an adaptation of TLS for datagram-based protocols, offers integrity, authenticity, and
confidentiality for messages in transit. The omdtls module is particularly useful in environments
where secure transmission of log data is crucial, such as in compliance-driven industries or when
transmitting across untrusted networks.

By operating over UDP, omdtls offers the benefits of lower latency and reduced protocol overhead
compared to TCP-based transport, making it well-suited for high-throughput logging scenarios or in
networks where connection-oriented protocols may face challenges.


Requirements
============

To send messages by DTLS you will need to fulfill the following requirements:

-  OpenSSL 1.0.2 or Higher


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omdtls-template`
     - .. include:: ../../reference/parameters/omdtls-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omdtls-template`
     - .. include:: ../../reference/parameters/omdtls-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-target`
     - .. include:: ../../reference/parameters/omdtls-target.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-port`
     - .. include:: ../../reference/parameters/omdtls-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-tls-authmode`
     - .. include:: ../../reference/parameters/omdtls-tls-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-tls-cacert`
     - .. include:: ../../reference/parameters/omdtls-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-tls-mycert`
     - .. include:: ../../reference/parameters/omdtls-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-tls-myprivkey`
     - .. include:: ../../reference/parameters/omdtls-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omdtls-tls-tlscfgcmd`
     - .. include:: ../../reference/parameters/omdtls-tls-tlscfgcmd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omdtls-template
   ../../reference/parameters/omdtls-target
   ../../reference/parameters/omdtls-port
   ../../reference/parameters/omdtls-tls-authmode
   ../../reference/parameters/omdtls-tls-cacert
   ../../reference/parameters/omdtls-tls-mycert
   ../../reference/parameters/omdtls-tls-myprivkey
   ../../reference/parameters/omdtls-tls-tlscfgcmd


.. _statistics-counter_omdtls_label:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omdtls that
accumulate all action instances. The statistic origin is named "omdtls" with following counters:


- **submitted** - This counter tracks the number of log messages that have been successfully send 
  by the current output instance.

- **failures** - This counter tracks the number of log messages that have been failed to send 
  to the target server.

These statistics counters are updated in real-time by the rsyslog output module as log data is processed,
and they provide valuable information about the performance and operation of the input module.

For multiple actions using statistics callback, there will be one record for each action.

.. _omdtls-examples-label:

Examples
========

Example 1: Basic
----------------

The following sample does the following:
-  loads the omdtls module
-  Sends all syslog messages to 192.168.2.1 by DTLS on port 4433.

.. code-block:: none

   module(load="omdtls")
   action(type="omdtls" name="omdtls" target="192.168.2.1" port="4433")


Example 2: Message throttling
-----------------------------

The following sample does the following:

-  loads the omdtls module
-  Sends all syslog messages to 192.168.2.1 by DTLS on port 4433.
-  Slows down sending to avoid package loss due the nature of UDP. In this sample,
   using dequeueSlowDown 1000 will limit the messages per second to 1000.

.. code-block:: none

   module(load="omdtls")
   action(type="omdtls"
       name="omdtls"
       target="192.168.2.1"
       port="4433"
       queue.type="FixedArray"
       queue.size="100000"
       queue.dequeueBatchSize="1"
       queue.minDequeueBatchSize.timeout="1000"
       queue.timeoutWorkerthreadShutdown="1000"
       queue.timeoutshutdown="1000"
       queue.dequeueSlowDown="1000"
   )
