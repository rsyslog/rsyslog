.. _param-imdocker-defaultseverity:
.. _imdocker.parameter.module.defaultseverity:

.. index::
   single: imdocker; DefaultSeverity
   single: DefaultSeverity

.. summary-start

Sets the syslog severity for received messages.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DefaultSeverity
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=notice
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the syslog severity to assign to log messages. Numeric severity codes may also be used.

.. _param-imdocker-module-defaultseverity:
.. _imdocker.parameter.module.defaultseverity-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" DefaultSeverity="...")

Notes
-----
Numeric severity codes may be specified instead of textual names.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdocker.parameter.legacy.inputfileseverity:

- $InputFileSeverity — maps to DefaultSeverity (status: legacy)

.. index::
   single: imdocker; $InputFileSeverity
   single: $InputFileSeverity

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
