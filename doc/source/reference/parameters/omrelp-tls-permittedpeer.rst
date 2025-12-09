.. _param-omrelp-tls-permittedpeer:
.. _omrelp.parameter.input.tls-permittedpeer:

TLS.PermittedPeer
=================

.. index::
   single: omrelp; TLS.PermittedPeer
   single: TLS.PermittedPeer

.. summary-start

Restricts which peers may connect based on expected names or fingerprints.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.PermittedPeer
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Note: this parameter is mandatory depending on the value of `TLS.AuthMode` but the code does currently not check this.

Peer Places access restrictions on this forwarder. Only peers which have been listed in this parameter may be connected to. This guards against rogue servers and man-in-the-middle attacks. The validation bases on the certificate the remote peer presents.

This contains either remote system names or fingerprints, depending on the value of parameter `TLS.AuthMode`. One or more values may be entered.

When a non-permitted peer is connected to, the refusal is logged together with the given remote peer identify. This is especially useful in *fingerprint* authentication mode: if the administrator knows this was a valid request, he can simply add the fingerprint by copy and paste from the logfile to rsyslog.conf. It must be noted, though, that this situation should usually not happen after initial client setup and administrators should be alert in this case.

Note that usually a single remote peer should be all that is ever needed. Support for multiple peers is primarily included in support of load balancing scenarios. If the connection goes to a specific server, only one specific certificate is ever expected (just like when connecting to a specific ssh server). To specify multiple fingerprints, just enclose them in braces like this:

.. code-block:: rsyslog

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string directly or enclose it in braces.

Note that in *name* authentication mode wildcards are supported. This can be done as follows:

.. code-block:: rsyslog

   tls.permittedPeer="*.example.com"

Of course, there can also be multiple names used, some with and some without wildcards:

.. code-block:: rsyslog

   tls.permittedPeer=["*.example.com", "srv1.example.net", "srv2.example.net"]

Input usage
-----------
.. _param-omrelp-input-tls-permittedpeer:
.. _omrelp.parameter.input.tls-permittedpeer-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.permittedPeer="*.example.com")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
