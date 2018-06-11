**************************
omrelp: RELP Output Module
**************************

===========================  ===========================================================================
**Module Name:**             **omrelp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module supports sending syslog messages over the reliable RELP
protocol. For RELP's advantages over plain tcp syslog, please see the
documentation for :doc:`imrelp <imrelp>` (the server counterpart). 

Setup

Please note that `librelp <http://www.librelp.com>`__ is required for
imrelp (it provides the core relp protocol implementation).


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Action Parameters
-----------------

Target
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The target server to connect to.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_ForwardFormat", "no", "none"

Defines the template to be used for the output.


Timeout
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "90", "no", "none"

Timeout for relp sessions. If set too low, valid sessions may be
considered dead and tried to recover.


Conn.Timeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "10", "no", "none"

Timeout for the socket connection.


RebindInterval
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "0", "no", "none"

Permits to specify an interval at which the current connection is
broken and re-established. This setting is primarily an aid to load
balancers. After the configured number of messages has been
transmitted, the current connection is terminated and a new one
started. This usually is perceived as a \`\`new connection'' by load
balancers, which in turn forward messages to another physical target
system.


WindowSize
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "0", "no", "none"

This is an **expert parameter**. It permits to override the RELP
window size being used by the client. Changing the window size has
both an effect on performance as well as potential message
duplication in failure case. A larger window size means more
performance, but also potentially more duplicated messages - and vice
versa. The default 0 means that librelp's default window size is
being used, which is considered a compromise between goals reached.
For your information: at the time of this writing, the librelp
default window size is 128 messages, but this may change at any time.
Note that there is no equivalent server parameter, as the client
proposes and manages the window size in RELP protocol.


TLS
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If set to "on", the RELP connection will be encrypted by TLS, so
that the data is protected against observers. Please note that both
the client and the server must have set TLS to either "on" or "off".
Other combinations lead to unpredictable results.

*Attention when using GnuTLS 2.10.x or older*

Versions older than GnuTLS 2.10.x may cause a crash (Segfault) under
certain circumstances. Most likely when an imrelp inputs and an
omrelp output is configured. The crash may happen when you are
receiving/sending messages at the same time. Upgrade to a newer
version like GnuTLS 2.12.21 to solve the problem.


TLS.Compression
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

The controls if the TLS stream should be compressed (zipped). While
this increases CPU use, the network bandwidth should be reduced. Note
that typical text-based log records usually compress rather well.


TLS.PermittedPeer
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

Peer Places access restrictions on this forwarder. Only peers which
have been listed in this parameter may be connected to. This guards
against rouge servers and man-in-the-middle attacks. The validation
bases on the certficate the remote peer presents.

The *peer* parameter lists permitted certificate fingerprints. Note
that it is an array parameter, so either a single or multiple
fingerprints can be listed. When a non-permitted peer is connected
to, the refusal is logged together with it's fingerprint. So if the
administrator knows this was a valid request, he can simple add the
fingerprint by copy and paste from the logfile to rsyslog.conf. It
must be noted, though, that this situation should usually not happen
after initial client setup and administrators should be alert in this
case.

Note that usually a single remote peer should be all that is ever
needed. Support for multiple peers is primarily included in support
of load balancing scenarios. If the connection goes to a specific
server, only one specific certificate is ever expected (just like
when connecting to a specific ssh server).
To specify multiple fingerprints, just enclose them in braces like
this:

.. code-block:: none

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string
directly or enclose it in braces.


TLS.AuthMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the mode used for mutual authentication. Supported values are
either "*fingerprint*\ " or "*name"*. Fingerprint mode basically is
what SSH does. It does not require a full PKI to be present, instead
self-signed certs can be used on all peers. Even if a CA certificate
is given, the validity of the peer cert is NOT verified against it.
Only the certificate fingerprint counts.
In "name" mode, certificate validation happens. Here, the matching is
done against the certificate's subjectAltName and, as a fallback, the
subject common name. If the certificate contains multiple names, a
match on any one of these names is considered good and permits the
peer to talk to rsyslog.


TLS.CaCert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The CA certificate that can verify the machine certs.


TLS.MyCert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The machine public certiificate.


TLS.MyPrivKey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The machine private key.


TLS.PriorityString
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This parameter permits to specify the so-called "priority string" to
GnuTLS. This string gives complete control over all crypto
parameters, including compression setting. For this reason, when the
prioritystring is specified, the "tls.compression" parameter has no
effect and is ignored.
Full information about how to construct a priority string can be
found in the GnuTLS manual. At the time of this writing, this
information was contained in `section 6.10 of the GnuTLS
manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`__.
**Note: this is an expert parameter.** Do not use if you do not
exactly know what you are doing.


LocalClientIp
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Omrelp uses ip_address as local client address while connecting
to remote logserver.


Examples
========

Sending msgs with omrelp
------------------------

The following sample sends all messages to the central server
"centralserv" at port 2514 (note that that server must run imrelp on
port 2514).

.. code-block:: none

   module(load="omrelp")
   action(type="omrelp" target="centralserv" port="2514")


|FmtObsoleteName| directives
============================

This module uses old-style action configuration to keep consistent with
the forwarding rule. So far, no additional configuration directives can
be specified. To send a message via RELP, use

.. code-block:: none

   *.*  :omrelp:<server>:<port>;<template>


