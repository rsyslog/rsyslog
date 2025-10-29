.. _param-imrelp-tls-permittedpeer:
.. _imrelp.parameter.input.tls-permittedpeer:

tls.permittedPeer
=================

.. index::
   single: imrelp; tls.permittedPeer
   single: tls.permittedPeer

.. summary-start

Restricts accepted clients to the listed certificate fingerprints or wildcard names.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.permittedPeer
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
The ``tls.permittedPeer`` setting places access restrictions on this listener.
Only peers which have been listed in this parameter may connect. The certificate
presented by the remote peer is used for it's validation.

The *peer* parameter lists permitted certificate fingerprints. Note that it is
an array parameter, so either a single or multiple fingerprints can be listed.
When a non-permitted peer connects, the refusal is logged together with it's
fingerprint. So if the administrator knows this was a valid request, he can
simply add the fingerprint by copy and paste from the logfile to rsyslog.conf.

To specify multiple fingerprints, just enclose them in braces like this:

.. code-block:: none

   tls.permittedPeer=["SHA1:...1", "SHA1:....2"]

To specify just a single peer, you can either specify the string directly or
enclose it in braces. You may also use wildcards to match a larger number of
permitted peers, e.g. ``*.example.com``.

When using wildcards to match larger number of permitted peers, please know that
the implementation is similar to Syslog RFC5425 which means: This wildcard
matches any left-most DNS label in the server name. That is, the subject
``*.example.com`` matches the server names ``a.example.com`` and
``b.example.com``, but does not match ``example.com`` or ``a.b.example.com``.

Input usage
-----------
.. _param-imrelp-input-tls-permittedpeer:
.. _imrelp.parameter.input.tls-permittedpeer-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514"
        tls="on"
        tls.permittedPeer=["SHA1:0123456789ABCDEF0123456789ABCDEF01234567"])

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
