.. _param-imtcp-streamdriver-keyfile:
.. _imtcp.parameter.input.streamdriver-keyfile:

StreamDriver.KeyFile
====================

.. index::
   single: imtcp; StreamDriver.KeyFile
   single: StreamDriver.KeyFile

.. summary-start

Overrides the ``DefaultNetstreamDriverKeyFile`` global parameter for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.KeyFile
:Scope: input
:Type: string
:Default: global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
This parameter permits overriding the ``DefaultNetstreamDriverKeyFile`` global parameter at the input level. It specifies the path to the private key file. For further details, see the documentation for the ``DefaultNetstreamDriverKeyFile`` global parameter.

Input usage
-----------
.. _imtcp.parameter.input.streamdriver-keyfile-usage:

.. code-block:: rsyslog

   input(type="imtcp"
         port="514"
         streamDriver.name="gtls"
         streamDriver.mode="1"
         streamDriver.keyFile="/path/to/key.pem")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
