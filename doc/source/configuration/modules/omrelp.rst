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

Module Parameters
-----------------

tls.tlslib
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.1903.0

Permits to specify the TLS library used by librelp.
All relp protocol operations or actually performed by librelp and
not rsyslog itself.  This value specified is directly passed down to
librelp. Depending on librelp version and build parameters, supported
tls libraries differ (or TLS may not be supported at all). In this case
rsyslog emits an error message.

Usually, the following options should be available: "openssl", "gnutls".

Note that "gnutls" is the current default for historic reasons. We actually
recommend to use "openssl". It provides better error messages and accepts
a wider range of certificate types.

If you have problems with the default setting, we recommend to switch to
"openssl".


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


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "514", "no", "none"

Name or numerical value of TCP port to use when connecting to target.


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

Note: this parameter is mandatory depending on the value of
`TLS.AuthMode` but the code does currently not check this.

Peer Places access restrictions on this forwarder. Only peers which
have been listed in this parameter may be connected to. This guards
against rogue servers and man-in-the-middle attacks. The validation
bases on the certificate the remote peer presents.

This contains either remote system names or fingerprints, depending
on the value of parameter `TLS.AuthMode`. One or more values may be
entered.

When a non-permitted peer is connected to, the refusal is logged
together with the given remote peer identify. This is especially
useful in *fingerprint* authentication mode: if the
administrator knows this was a valid request, he can simply add the
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

Note that in *name* authentication mode wildcards are supported.
This can be done as follows:

.. code-block:: none

   tls.permittedPeer="*.example.com"

Of course, there can also be multiple names used, some with and
some without wildcards:

.. code-block:: none

   tls.permittedPeer=["*.example.com", "srv1.example.net", "srv2.example.net"]


TLS.AuthMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the mode used for mutual authentication. Supported values are
either "*fingerprint*" or "*name*". Fingerprint mode basically is
what SSH does. It does not require a full PKI to be present, instead
self-signed certs can be used on all peers. Even if a CA certificate
is given, the validity of the peer cert is NOT verified against it.
Only the certificate fingerprint counts.

In "name" mode, certificate validation happens. Here, the matching is
done against the certificate's subjectAltName and, as a fallback, the
subject common name. If the certificate contains multiple names, a
match on any one of these names is considered good and permits the
peer to talk to rsyslog.

The permitted names or fingerprints are configured via
`TLS.PermittedPeer`.


About Chained Certificates
--------------------------

.. versionadded:: 8.2008.0

With librelp 1.7.0, you can use chained certificates.
If using "openssl" as tls.tlslib, we recommend at least OpenSSL Version 1.1
or higher. Chained certificates will also work with OpenSSL Version 1.0.2, but
they will be loaded into the main OpenSSL context object making them available
to all librelp instances (omrelp/imrelp) within the same process.

If this is not desired, you will require to run rsyslog in multiple instances
with different omrelp configurations and certificates.


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

The machine public certificate.


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

tls.tlscfgcmd 
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.2001.0

The setting can be used if tls.tlslib is set to "openssl" to pass configuration commands to
the openssl library.
OpenSSL Version 1.0.2 or higher is required for this feature.
A list of possible commands and their valid values can be found in the documentation:
https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/

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
"centralserv" at port 2514 (note that the server must run imrelp on
port 2514).

.. code-block:: none

   module(load="omrelp")
   action(type="omrelp" target="centralserv" port="2514")


Sending msgs with omrelp via TLS
------------------------------------

This is the same as the previous example but uses TLS (via OpenSSL) for
operations.

Certificate files must exist at configured locations. Note that authmode
"certvalid" is not very strong - you may want to use a different one for
actual deployments. For details, see parameter descriptions.

.. code-block:: none

   module(load="omrelp" tls.tlslib="openssl")
   action(type="omrelp"
		target="centralserv" port="2514" tls="on"
		tls.cacert="tls-certs/ca.pem"
		tls.mycert="tls-certs/cert.pem"
		tls.myprivkey="tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")


|FmtObsoleteName| directives
============================

This module uses old-style action configuration to keep consistent with
the forwarding rule. So far, no additional configuration directives can
be specified. To send a message via RELP, use

.. code-block:: none

   *.*  :omrelp:<server>:<port>;<template>


