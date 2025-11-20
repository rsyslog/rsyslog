.. _param-omlibdbi-driverdirectory:
.. _omlibdbi.parameter.module.driverdirectory:

DriverDirectory
===============

.. index::
   single: omlibdbi; DriverDirectory
   single: DriverDirectory

.. summary-start

Points libdbi to the directory that contains its driver shared objects.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: DriverDirectory
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not documented

Description
-----------
Set this global option only when libdbi cannot find its drivers
automatically. It tells libdbi where the driver modules are installed.

Most systems ship libdbi-driver packages that register the directory
properly. Only installations that place drivers in nonstandard paths need
this parameter.

Module usage
------------
.. _param-omlibdbi-module-driverdirectory-usage:
.. _omlibdbi.parameter.module.driverdirectory-usage:

.. code-block:: rsyslog

   module(load="omlibdbi" driverDirectory="/usr/lib/libdbi")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbidriverdirectory:

- $ActionLibdbiDriverDirectory — maps to DriverDirectory (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiDriverDirectory
   single: $ActionLibdbiDriverDirectory

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
