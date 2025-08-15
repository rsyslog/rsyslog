.. _param-imtcp-streamdriver-certfile:
.. _imtcp.parameter.input.streamdriver-certfile:

streamDriver.CertFile
=====================

.. index::
   single: imtcp; streamDriver.CertFile
   single: streamDriver.CertFile

.. summary-start

Overrides ``DefaultNetstreamDriverCertFile`` for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: streamDriver.CertFile
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
.. versionadded:: 8.2108.0

This permits to override the ``DefaultNetstreamDriverCertFile`` global parameter on the ``input()``
level. For further details, see the global parameter.

Input usage
-----------
.. _param-imtcp-input-streamdriver-certfile:
.. _imtcp.parameter.input.streamdriver-certfile-usage:

.. code-block:: rsyslog

   input(type="imtcp" streamDriver.certFile="/etc/ssl/certs/cert.pem")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
