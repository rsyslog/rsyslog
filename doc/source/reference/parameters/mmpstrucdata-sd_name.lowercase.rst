.. _param-mmpstrucdata-sd_name.lowercase:
.. _mmpstrucdata.parameter.action.sd_name.lowercase:

sd_name.lowercase
=================

.. index::
   single: mmpstrucdata; sd_name.lowercase
   single: sd_name.lowercase

.. summary-start

Controls whether structured data element names (SDIDs) are lowercased.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmpstrucdata`.

:Name: sd_name.lowercase
:Scope: action
:Type: boolean
:Default: action="on"
:Required?: no
:Introduced: 8.32.0

Description
-----------
Specifies if sd names (SDID) shall be lowercased. If set to "on", this is
the case; if "off", then not. The default of "on" is used because that was the
traditional mode of operations. It is generally advised to change the
parameter to "off" if not otherwise required.

Action usage
------------
.. _param-mmpstrucdata-action-sd_name.lowercase:
.. _mmpstrucdata.parameter.action.sd_name.lowercase-usage:

.. code-block:: rsyslog

   action(type="mmpstrucdata" sd_name.lowercase="off")

See also
--------
See also :doc:`../../configuration/modules/mmpstrucdata`.
