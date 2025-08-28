.. _param-imtcp-streamdriver-crlfile:
.. _imtcp.parameter.input.streamdriver-crlfile:

streamDriver.CRLFile
====================

.. index::
   single: imtcp; streamDriver.CRLFile
   single: streamDriver.CRLFile

.. summary-start

Overrides the CRL file set via the global configuration.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: streamDriver.CRLFile
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=global parameter
:Required?: no
:Introduced: 8.2308.0

Description
-----------
.. versionadded:: 8.2308.0

This permits to override the CRL (Certificate revocation list) file set via ``global()``
config object at the per-action basis. This parameter is ignored if the netstream driver
and/or its mode does not need or support certificates.

Input usage
-----------
.. _param-imtcp-input-streamdriver-crlfile:
.. _imtcp.parameter.input.streamdriver-crlfile-usage:

.. code-block:: rsyslog

   input(type="imtcp" streamDriver.crlFile="/etc/ssl/crl.pem")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
