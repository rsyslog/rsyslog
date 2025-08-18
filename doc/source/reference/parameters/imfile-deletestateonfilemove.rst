.. _param-imfile-deletestateonfilemove:
.. _imfile.parameter.input.deletestateonfilemove:
.. _imfile.parameter.deletestateonfilemove:

deleteStateOnFileMove
=====================

.. index::
   single: imfile; deleteStateOnFileMove
   single: deleteStateOnFileMove

.. summary-start

Removes the state file when a monitored file is moved or renamed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: deleteStateOnFileMove
:Scope: module, input
:Type: boolean
:Default: module=off; input=inherits module
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When set to ``on`` at module scope, the state file for a monitored file is
removed when that file is moved or rotated away. By default the state file is
kept.

The input parameter controls whether the state file is deleted when its
associated file is moved or renamed. It inherits the module setting unless
explicitly configured. Enabling it prevents orphaned state files.

Module usage
------------
.. _param-imfile-module-deletestateonfilemove:
.. _imfile.parameter.module.deletestateonfilemove-usage:

.. code-block:: rsyslog

   module(load="imfile" deleteStateOnFileMove="on")

Input usage
-----------
.. _param-imfile-input-deletestateonfilemove:
.. _imfile.parameter.input.deletestateonfilemove-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         deleteStateOnFileMove="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated
  as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
