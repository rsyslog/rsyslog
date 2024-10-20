*************************
imrelp: RELP Input Module
*************************

===========================  ===========================================================================
**Module Name:**             **imrelp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via the reliable RELP
protocol. This module requires `librelp <http://www.librelp.com>`__ to
be present on the system. From the user's point of view, imrelp works
much like imtcp or imgssapi, except that no message loss can occur.
Please note that with the currently supported RELP protocol version, a
minor message duplication may occur if a network connection between the
relp client and relp server breaks after the client could successfully
send some messages but the server could not acknowledge them. The window
of opportunity is very slim, but in theory this is possible. Future
versions of RELP will prevent this. Please also note that rsyslogd may
lose a few messages if rsyslog is shutdown while a network connection to
the server is broken and could not yet be recovered. Future versions of
RELP support in rsyslog will prevent that issue. Please note that both
scenarios also exist with plain TCP syslog. RELP, even with the small
nits outlined above, is a much more reliable solution than plain TCP
syslog and so it is highly suggested to use RELP instead of plain TCP.
Clients send messages to the RELP server via omrelp.


Notable Features
================

- :ref:`imrelp-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$InputRELPServerBindRuleset``"

.. versionadded:: 7.5.0

Binds the specified ruleset to **all** RELP listeners. This can be
overridden at the instance level.

tls.tlslib
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

.. versionadded:: 8.1903.0

Permits to specify the TLS library used by librelp.
All RELP protocol operations are actually performed by librelp and
not rsyslog itself.  The value specified is directly passed down to
librelp. Depending on librelp version and build parameters, supported
TLS libraries differ (or TLS may not be supported at all). In this case
rsyslog emits an error message.

Usually, the following options should be available: "openssl", "gnutls".

Note that "gnutls" is the current default for historic reasons. We actually
recommend to use "openssl". It provides better error messages and accepts
a wider range of certificate types.

If you have problems with the default setting, we recommend to switch to
"openssl".


Input Parameters
----------------

Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "yes", "``$InputRELPServerRun``"

Starts a RELP server on selected port

Address
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

.. versionadded:: 8.37.0

Bind the RELP server to that address. If not specified, the server will be
bound to the wildcard address.

Name
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "imrelp", "no", "none"


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Binds specified ruleset to this listener.  This overrides the
module-level Ruleset parameter.


MaxDataSize
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "size_nbr", ":doc:`global(maxMessageSize) <../../rainerscript/global>`", "no", "none"

Sets the max message size (in bytes) that can be received. Messages that
are too long are handled as specified in parameter oversizeMode. Note that
maxDataSize cannot be smaller than the global parameter maxMessageSize.


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


TLS.dhbits
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

This setting controls how many bits are used for Diffie-Hellman key
generation. If not set, the librelp default is used. For security
reasons, at least 1024 bits should be used. Please note that the
number of bits must be supported by GnuTLS. If an invalid number is
given, rsyslog will report an error when the listener is started. We
do this to be transparent to changes/upgrades in GnuTLS (to check at
config processing time, we would need to hardcode the supported bits
and keep them in sync with GnuTLS - this is even impossible when
custom GnuTLS changes are made...).


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


TLS.AuthMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Sets the mode used for mutual authentication.

Supported values are either "*fingerprint*\ " or "*name"*.

Fingerprint mode basically is what SSH does. It does not require a
full PKI to be present, instead self-signed certs can be used on all
peers. Even if a CA certificate is given, the validity of the peer
cert is NOT verified against it. Only the certificate fingerprint
counts.

In "name" mode, certificate validation happens. Here, the matching is
done against the certificate's subjectAltName and, as a fallback, the
subject common name. If the certificate contains multiple names, a
match on any one of these names is considered good and permits the
peer to talk to rsyslog.


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

The CA certificate that is being used to verify the client certificates.
Has to be configured if TLS.AuthMode is set to "*fingerprint*\ " or "*name"*.


TLS.MyCert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The machine certificate that is being used for TLS communication.


TLS.MyPrivKey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

The machine private key for the configured TLS.MyCert.


TLS.PriorityString
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

This parameter allows passing the so-called "priority string" to
GnuTLS. This string gives complete control over all crypto
parameters, including compression settings. For this reason, when the
prioritystring is specified, the "tls.compression" parameter has no
effect and is ignored.

Full information about how to construct a priority string can be
found in the GnuTLS manual. At the time of writing, this
information was contained in `section 6.10 of the GnuTLS
manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`_.

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
the openssl libray.
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


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Enable or disable keep-alive packets at the TCP socket layer. By 
default keep-alives are disabled.


KeepAlive.Probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The number of keep-alive probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This only has an 
effect if keep-alives are enabled. The functionality may not be
available on all platforms.


KeepAlive.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The interval between subsequent keep-alive probes, regardless of what
the connection has been exchanged in the meantime. The default, 0, 
means that the operating system defaults are used. This only has an effect 
if keep-alive is enabled. The functionality may not be available on all
platforms.


KeepAlive.Time
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The interval between the last data packet sent (simple ACKs are not
considered data) and the first keepalive probe; after the connection
is marked with keep-alive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This only has an effect if keep-alive is enabled. The functionality may
not be available on all platforms.


oversizeMode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "truncate", "no", "none"

.. versionadded:: 8.35.0

This parameter specifies how messages that are too long will be handled.
For this parameter the length of the parameter maxDataSize is used.

- truncate: Messages will be truncated to the maximum message size.
- abort: This is the behaviour until version 8.35.0. Upon receiving a
  message that is too long imrelp will abort.
- accept: Messages will be accepted even if they are too long and an error
  message will be output. Using this option does have associated risks.


flowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "light", "no", "none"

.. versionadded:: 8.1911.0


This parameter permits the fine-tuning of the flowControl parameter.
Possible values are "no", "light", and "full". With "light" being the default
and previously only value.

Changing the flow control setting may be useful for some rare applications,
this is an advanced setting and should only be changed if you know what you
are doing. Most importantly, **rsyslog block incoming data and become 
unresponsive if you change flowcontrol to "full"**. While this may be a 
desired effect when intentionally trying to make it most unlikely that 
rsyslog needs to lose/discard messages, usually this is not what you want.

General rule of thumb: **if you do not fully understand what this description
here talks about, leave the parameter at default value**.

This part of the
documentation is intentionally brief, as one needs to have deep understanding
of rsyslog to evaluate usage of this parameter. If someone has the insight,
the meaning of this parameter is crystal-clear. If not, that someone will
most likely make the wrong decision when changing this parameter away
from the default value.


.. _imrelp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener.
The statistic by default is named "imrelp" , followed by the listener port in
parenthesis. For example, the counter for a listener on port 514 is called "imprelp(514)".
If the input is given a name, that input name is used instead of "imrelp". This counter is
available starting rsyslog 7.5.1

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


Caveats/Known Bugs
==================

-  see description
-  To obtain the remote system's IP address, you need to have at least
   librelp 1.0.0 installed. Versions below it return the hostname
   instead of the IP address.


Examples
========

Example 1
---------

This sets up a RELP server on port 2514 with a max message size of 10,000 bytes.

.. code-block:: none

   module(load="imrelp") # needs to be done just once
   input(type="imrelp" port="2514" maxDataSize="10k")



Receive RELP traffic via TLS
----------------------------

This receives RELP traffic via TLS using the recommended "openssl" library.
Except for encryption support the scenario is the same as in Example 1.

Certificate files must exist at configured locations. Note that authmode
"certvalid" is not very strong - you may want to use a different one for
actual deployments. For details, see parameter descriptions.

.. code-block:: none

   module(load="imrelp" tls.tlslib="openssl")
   input(type="imrelp" port="2514" maxDataSize="10k"
                tls="on"
		tls.cacert="/tls-certs/ca.pem"
		tls.mycert="/tls-certs/cert.pem"
		tls.myprivkey="/tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")

