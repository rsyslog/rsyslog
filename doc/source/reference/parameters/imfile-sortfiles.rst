.. _param-imfile-sortfiles:
.. _imfile.parameter.module.sortfiles:
.. _imfile.parameter.sortfiles:

sortFiles
=========

.. index::
   single: imfile; sortFiles
   single: sortFiles

.. summary-start

Processes files in sorted order when enabled.

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
If this parameter is set to ``on``, the files will be processed in sorted order.
Due to the inherent asynchronicity of the operations involved in tracking files,
it is not possible to guarantee this sorted order, as it also depends on
operation mode and OS timing.

Module usage
------------
.. _param-imfile-module-sortfiles:
.. _imfile.parameter.module.sortfiles-usage:

.. code-block:: rsyslog

   module(load="imfile" sortFiles="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated
  as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
