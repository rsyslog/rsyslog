.. _param-imtcp-streamdriver-certfile:
.. _imtcp.parameter.input.streamdriver-certfile:

StreamDriver.CertFile
=====================

.. index::
   single: imtcp; StreamDriver.CertFile
   single: StreamDriver.CertFile

.. summary-start

Overrides the ``DefaultNetstreamDriverCertFile`` global parameter for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.CertFile
:Scope: input
:Type: string
:Default: global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
This parameter permits overriding the ``DefaultNetstreamDriverCertFile`` global parameter at the input level. It specifies the path to the certificate file. For further details, see the documentation for the ``DefaultNetstreamDriverCertFile`` global parameter.

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-certfile-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.certFile="/path/to/cert.pem")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
