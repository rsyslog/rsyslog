.. _param-imtcp-streamdriver-keyfile:
.. _imtcp.parameter.input.streamdriver-keyfile:

streamDriver.KeyFile
====================

.. index::
   single: imtcp; streamDriver.KeyFile
   single: streamDriver.KeyFile

.. summary-start

Overrides ``DefaultNetstreamDriverKeyFile`` for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: streamDriver.KeyFile
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
.. versionadded:: 8.2108.0

This permits to override the ``DefaultNetstreamDriverKeyFile`` global parameter on the ``input()``
level. For further details, see the global parameter.

Input usage
-----------
.. _param-imtcp-input-streamdriver-keyfile:
.. _imtcp.parameter.input.streamdriver-keyfile-usage:

.. code-block:: rsyslog

   input(type="imtcp" streamDriver.keyFile="/etc/ssl/private/key.pem")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
