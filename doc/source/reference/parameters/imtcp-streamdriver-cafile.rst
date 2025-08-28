.. _param-imtcp-streamdriver-cafile:
.. _imtcp.parameter.input.streamdriver-cafile:

streamDriver.CAFile
===================

.. index::
   single: imtcp; streamDriver.CAFile
   single: streamDriver.CAFile

.. summary-start

Overrides ``DefaultNetstreamDriverCAFile`` for this input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: streamDriver.CAFile
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=global parameter
:Required?: no
:Introduced: 8.2108.0

Description
-----------
.. versionadded:: 8.2108.0

This permits to override the ``DefaultNetstreamDriverCAFile`` global parameter on the ``input()``
level. For further details, see the global parameter.

Input usage
-----------
.. _param-imtcp-input-streamdriver-cafile:
.. _imtcp.parameter.input.streamdriver-cafile-usage:

.. code-block:: rsyslog

   input(type="imtcp" streamDriver.caFile="/etc/ssl/certs/ca.pem")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
