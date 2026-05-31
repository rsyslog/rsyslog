.. _param-mmpstrucdata-container:
.. _mmpstrucdata.parameter.action.container:

container
=========

.. index::
   single: mmpstrucdata; container
   single: container

.. summary-start

Sets the JSON object member name that receives parsed structured data.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmpstrucdata`.

:Name: container
:Scope: action
:Type: string
:Default: action="rfc5424-sd"
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Specifies the JSON member name below :ref:`param-mmpstrucdata-jsonroot` where
parsed RFC5424 structured data is stored. This parameter changes the member
name only; it is not a JSON path. Use ``jsonRoot`` to move the parent location.

When an RFC5424 message has no structured data, the module creates this member
with a JSON ``null`` value.

Action usage
------------
.. _param-mmpstrucdata-action-container:
.. _mmpstrucdata.parameter.action.container-usage:

.. code-block:: rsyslog

   action(type="mmpstrucdata" jsonRoot="$!structured-data" container="sd")

YAML usage
----------
.. code-block:: yaml

   actions:
     - type: mmpstrucdata
       jsonRoot: "$!structured-data"
       container: sd

See also
--------
See also :doc:`../../configuration/modules/mmpstrucdata`.
