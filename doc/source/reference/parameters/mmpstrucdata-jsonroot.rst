.. _param-mmpstrucdata-jsonroot:
.. _mmpstrucdata.parameter.module.jsonroot:

JsonRoot
========

.. index::
   single: mmpstrucdata; JsonRoot
   single: JsonRoot

.. summary-start

Sets the JSON root container where parsed structured data is stored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmpstrucdata`.

:Name: JsonRoot
:Scope: action
:Type: string
:Default: action="!"
:Required?: no
:Introduced: 7.5.4

Description
-----------
Specifies into which json container the data shall be parsed to.

Action usage
------------
.. _param-mmpstrucdata-action-jsonroot:
.. _mmpstrucdata.parameter.action.jsonroot-usage:

.. code-block:: rsyslog

   action(type="mmpstrucdata" jsonRoot="!")

See also
--------
See also :doc:`../../configuration/modules/mmpstrucdata`.
