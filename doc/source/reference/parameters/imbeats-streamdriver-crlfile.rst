.. _param-imbeats-streamdriver-crlfile:
.. _imbeats.parameter.input.streamdriver-crlfile:

StreamDriver.CRLFile
====================

.. meta::
   :description: CRL file for imbeats TLS validation.
   :keywords: rsyslog, imbeats, streamdriver crlfile

.. index::
   single: imbeats; StreamDriver.CRLFile
   single: StreamDriver.CRLFile

.. summary-start

Specify the certificate revocation list file used by TLS-enabled imbeats listeners.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.CRLFile
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Specify the certificate revocation list file used by TLS-enabled imbeats listeners.

Input usage
-----------
.. _param-imbeats-input-streamdriver-crlfile:
.. _imbeats.parameter.input.streamdriver-crlfile-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.crlFile="/etc/rsyslog/crl.pem")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
