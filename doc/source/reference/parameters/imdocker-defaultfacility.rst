.. _param-imdocker-defaultfacility:
.. _imdocker.parameter.module.defaultfacility:

.. index::
   single: imdocker; DefaultFacility
   single: DefaultFacility

.. summary-start

Sets the syslog facility for received messages.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DefaultFacility
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=user
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the syslog facility to assign to log messages. Numeric facility codes may also be used.

.. _param-imdocker-module-defaultfacility:
.. _imdocker.parameter.module.defaultfacility-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" DefaultFacility="...")

Notes
-----
Numeric facility codes may be specified instead of textual names.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdocker.parameter.legacy.inputfilefacility:

- $InputFileFacility — maps to DefaultFacility (status: legacy)

.. index::
   single: imdocker; $InputFileFacility
   single: $InputFileFacility

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
