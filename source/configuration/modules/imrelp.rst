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
Please note that with the currently supported relp protocol version, a
minor message duplication may occur if a network connection between the
relp client and relp server breaks after the client could successfully
send some messages but the server could not acknowledge them. The window
of opportunity is very slim, but in theory this is possible. Future
versions of RELP will prevent this. Please also note that rsyslogd may
lose a few messages if rsyslog is shutdown while a network connection to
the server is broken and could not yet be recovered. Future version of
RELP support in rsyslog will prevent that. Please note that both
scenarios also exists with plain tcp syslog. RELP, even with the small
nits outlined above, is a much more reliable solution than plain tcp
syslog and so it is highly suggested to use RELP instead of plain tcp.
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
generation. If not set, the librelp default is used. For secrity
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

Peer places access restrictions on this listener. Only peers which
have been listed in this parameter may connect. The validation bases
on the certificate the remote peer presents.

The *peer* parameter lists permitted certificate fingerprints. Note
that it is an array parameter, so either a single or multiple
fingerprints can be listed. When a non-permitted peer connects, the
refusal is logged together with it's fingerprint. So if the
administrator knows this was a valid request, he can simple add the
fingerprint by copy and paste from the logfile to rsyslog.conf.

To specify multiple fingerprints, just enclose them in braces like
this:

.. code-block:: none

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string
directly or enclose it in braces. You may also use wildcards to match
a larger number of permitted peers, e.g. ``*.example.com``.

When using wildcards to match larger number of permitted peers, please
know that the implementation is simular to Syslog RFC5425 which means:
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

The machine certificate that is being used for TLS communciation.


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

This parameter permits to specify the so-called "priority string" to
GnuTLS. This string gives complete control over all crypto
parameters, including compression setting. For this reason, when the
prioritystring is specified, the "tls.compression" parameter has no
effect and is ignored.

Full information about how to construct a priority string can be
found in the GnuTLS manual. At the time of this writing, this
information was contained in `section 6.10 of the GnuTLS
manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`_.

**Note: this is an expert parameter.** Do not use if you do not
exactly know what you are doing.


KeepAlive
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Enable of disable keep-alive packets at the tcp socket layer. The
default is to disable them.


KeepAlive.Probes
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The number of unacknowledged probes to send before considering the
connection dead and notifying the application layer. The default, 0,
means that the operating system defaults are used. This has only
effect if keep-alive is enabled. The functionality may not be
available on all platforms.


KeepAlive.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

The interval between subsequent keepalive probes, regardless of what
the connection has exchanged in the meantime. The default, 0, means
that the operating system defaults are used. This has only effect if
keep-alive is enabled. The functionality may not be available on all
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
is marked to need keepalive, this counter is not used any further.
The default, 0, means that the operating system defaults are used.
This has only effect if keep-alive is enabled. The functionality may
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

- truncate: Messages will be truncated at the maximal message size.
- abort: This is the behaviour until version 8.35.0. Upon receiving a
  message that is too long imrelp will abort.
- accept: Messages will be accepted even if they are too long and an error
  message will be put out. Using this option will bring some risks with it.


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

This sets up a RELP server on port 20514 with a max message size of 10,000 bytes.

.. code-block:: none

   module(load="imrelp") # needs to be done just once
   input(type="imrelp" port="20514" maxDataSize="10k")


