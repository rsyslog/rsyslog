.. _param-imtcp-streamdriver-cafile:
.. _imtcp.parameter.input.streamdriver-cafile:

StreamDriver.CAFile
===================

.. index::
   single: imtcp; StreamDriver.CAFile
   single: StreamDriver.CAFile

.. summary-start

Overrides the ``DefaultNetstreamDriverCAFile`` global parameter for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.CAFile
:Scope: input
:Type: string
:Default: global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
This parameter permits overriding the ``DefaultNetstreamDriverCAFile`` global parameter at the input level. It specifies the path to the Certificate Authority (CA) file used to verify peer certificates. For further details, see the documentation for the ``DefaultNetstreamDriverCAFile`` global parameter.

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-cafile-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.caFile="/path/to/ca.pem")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
