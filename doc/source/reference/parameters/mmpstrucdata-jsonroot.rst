.. _param-mmpstrucdata-jsonroot:
.. _mmpstrucdata.parameter.action.jsonroot:

jsonRoot
========

.. index::
   single: mmpstrucdata; jsonRoot
   single: jsonRoot

.. summary-start

Sets the JSON root container where parsed structured data is stored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmpstrucdata`.

:Name: jsonRoot
:Scope: action
:Type: string
:Default: action="!"
:Required?: no
:Introduced: 7.5.4

Description
-----------
Specifies into which JSON container the module writes its structured-data
object. The object member name below this root is controlled by
:ref:`param-mmpstrucdata-container`.

Action usage
------------
.. _param-mmpstrucdata-action-jsonroot:
.. _mmpstrucdata.parameter.action.jsonroot-usage:

.. code-block:: rsyslog

   action(type="mmpstrucdata" jsonRoot="!")

YAML usage
----------
.. code-block:: yaml

   actions:
     - type: mmpstrucdata
       jsonRoot: "!"

See also
--------
See also :doc:`../../configuration/modules/mmpstrucdata`.
