.. _param-imtcp-streamdriver-crlfile:
.. _imtcp.parameter.input.streamdriver-crlfile:

StreamDriver.CRLFile
====================

.. index::
   single: imtcp; StreamDriver.CRLFile
   single: StreamDriver.CRLFile

.. summary-start

Overrides the global Certificate Revocation List (CRL) file for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.CRLFile
:Scope: input
:Type: string
:Default: global parameter
:Required?: no
:Introduced: 8.2308.0

Description
-----------
This parameter permits overriding the Certificate Revocation List (CRL) file set via the ``global()`` configuration object on a per-input basis. This parameter is ignored if the netstream driver and/or its mode does not need or support certificates.

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-crlfile-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.crlFile="/path/to/crl.pem")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
