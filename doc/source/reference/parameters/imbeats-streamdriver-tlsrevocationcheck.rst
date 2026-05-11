.. _param-imbeats-streamdriver-tlsrevocationcheck:
.. _imbeats.parameter.input.streamdriver-tlsrevocationcheck:

StreamDriver.TlsRevocationCheck
===============================

.. meta::
   :description: Revocation checking for imbeats TLS peers.
   :keywords: rsyslog, imbeats, tls revocation check

.. index::
   single: imbeats; StreamDriver.TlsRevocationCheck
   single: StreamDriver.TlsRevocationCheck

.. summary-start

Enable TLS revocation checking for certificates presented to the imbeats listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.TlsRevocationCheck
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Enable TLS revocation checking for certificates presented to the imbeats listener.

Input usage
-----------
.. _param-imbeats-input-streamdriver-tlsrevocationcheck:
.. _imbeats.parameter.input.streamdriver-tlsrevocationcheck-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.tlsRevocationCheck="on")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
