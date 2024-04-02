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

   Parameter names are case-insensitive.


Action Parameters
-----------------

address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Specifies the IP address on where the imdtls module will listen for
incoming syslog messages. By default the module will listen on all available
network connections.


port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "4433", "yes", "none"

Specifies the UDP port to which the imdtls module will bind and listen for
incoming connections. The default port number for DTLS is 4433.


timeout
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "1800", "no", "none"

Specifies the DTLS session timeout. As DTLS runs on transportless UDP protocol, there are no
automatic detections of a session timeout. The input will close the DTLS session if no data
is received from the client for the configured timeout period. The default is 1800 seconds
which is equal to 30 minutes.


name
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive", "Available since"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Unique name to the input module instance. This is useful for identifying the source of
messages when multiple input modules are used.


ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Determines the ruleset to which the imdtls input will be bound to. This can be
overridden at the instance level.


tls.AuthMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the mode used for mutual authentication.

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

Specifies the certificate file used by imdtls.
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


TLS.PermittedPeer
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

PermittedPeer places access restrictions on this listener. Only peers which
have been listed in this parameter may connect. The certificate presented 
by the remote peer is used for it's validation. 

The *peer* parameter lists permitted certificate fingerprints. Note
that it is an array parameter, so either a single or multiple
fingerprints can be listed. When a non-permitted peer connects, the
refusal is logged together with it's fingerprint. So if the
administrator knows this was a valid request, he can simply add the
fingerprint by copy and paste from the logfile to rsyslog.conf.

To specify multiple fingerprints, just enclose them in braces like
this:

.. code-block:: none

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string
directly or enclose it in braces. You may also use wildcards to match
a larger number of permitted peers, e.g. ``*.example.com``.

When using wildcards to match larger number of permitted peers, please
know that the implementation is similar to Syslog RFC5425 which means:
This wildcard matches any left-most DNS label in the server name.
That is, the subject ``*.example.com`` matches the server names ``a.example.com``
and ``b.example.com``, but does not match ``example.com`` or ``a.b.example.com``.


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

