.. _param-imdtls-tls-permittedpeer:
.. _imdtls.parameter.input.tls-permittedpeer:

TLS.PermittedPeer
=================

.. index::
   single: imdtls; TLS.PermittedPeer
   single: TLS.PermittedPeer

.. summary-start


Restricts DTLS clients to the listed certificate fingerprints or names.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: TLS.PermittedPeer
:Scope: input
:Type: array
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
``TLS.PermittedPeer`` places access restrictions on this listener. Only peers whose certificate fingerprint or name is listed in this array parameter may connect. The certificate presented by the remote peer is used for its validation.

When a non-permitted peer connects, the refusal is logged together with its fingerprint. If the administrator knows this was a valid request, they can simply add the fingerprint by copy and paste from the logfile to ``rsyslog.conf``.

To specify multiple fingerprints, enclose them in braces like this:

.. code-block:: none

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string directly or enclose it in braces. You may also use wildcards to match a larger number of permitted peers, e.g. ``*.example.com``.

When using wildcards to match a larger number of permitted peers, the implementation is similar to Syslog RFC5425. This wildcard matches any left-most DNS label in the server name. That is, the subject ``*.example.com`` matches the server names ``a.example.com`` and ``b.example.com``, but does not match ``example.com`` or ``a.b.example.com``.

Input usage
-----------
.. _param-imdtls-input-tls-permittedpeer:
.. _imdtls.parameter.input.tls-permittedpeer-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls"
         tls.permittedPeer=["SHA1:11223344556677889900AABBCCDDEEFF00112233"])

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
