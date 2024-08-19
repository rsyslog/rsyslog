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

   Parameter names are case-insensitive.

Module Parameters
-----------------

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "``$ActionForwardDefaultTemplateName``"

Sets a non-standard default template for this module.


Action Parameters
-----------------

target
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Specifies the target hostname or IP address to send log messages to.


port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "4433", "yes", "none"

Defines the port number on the target host where log messages will be sent.
The default port number for DTLS is 4433.


tls.AuthMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the mode of authentication to be used.
Supported values are either "*fingerprint*\ ", "*name"* or "*certvalid*\ ".

Fingerprint: Authentication based on certificate fingerprint.
Name: Authentication based on the subjectAltName and, as a fallback, the
subject common name.
Certvalid: Requires a valid certificate for authentication.
Certanon: Anything else will allow anonymous authentication (no client certificate).


tls.cacert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The CA certificate that is being used to verify the client certificates.
Has to be configured if tls.authmode is set to "*fingerprint*\ ", "*name"* or "*certvalid*\ ".


tls.mycert 
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Specifies the certificate file used by omdtls.
This certificate is presented to peers during the DTLS handshake.


tls.myprivkey 
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The private key file corresponding to tls.mycert.
This key is used for the cryptographic operations in the DTLS handshake.


tls.tlscfgcmd 
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Used to pass additional OpenSSL configuration commands. This can be used to fine-tune the OpenSSL
settings by passing configuration commands to the openssl libray.
OpenSSL Version 1.0.2 or higher is required for this feature.
A list of possible commands and their valid values can be found in the documentation:
https://www.openssl.org/docs/man1.0.2/man3/SSL_CONF_cmd.html

The setting can be single or multiline, each configuration command is separated by linefeed (\n).
Command and value are separated by equal sign (=). Here are a few samples:

Example 1
---------

This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3"


Example 2
---------

This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", ""

Sets a non-standard default template for this action instance.


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
