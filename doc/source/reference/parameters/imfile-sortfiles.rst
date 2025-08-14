.. _param-imfile-sortfiles:
.. _imfile.parameter.module.sortfiles:

sortFiles
=========

.. index::
   single: imfile; sortFiles
   single: sortFiles

.. summary-start

.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: sortFiles
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.32.0

Description
-----------
.. versionadded:: 8.32.0

If this parameter is set to on, the files will be processed in sorted order, else
not. However, due to the inherent asynchronicity of the whole operations involved
in tracking files, it is not possible to guarantee this sorted order, as it also
depends on operation mode and OS timing.

Module usage
------------
.. _param-imfile-module-sortfiles:
.. _imfile.parameter.module.sortfiles-usage:
.. code-block:: rsyslog

   module(load="imfile" sortFiles="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
