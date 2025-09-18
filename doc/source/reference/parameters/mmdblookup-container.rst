.. _param-mmdblookup-container:
.. _mmdblookup.parameter.module.container:

Container
=========

.. index::
   single: mmdblookup; Container
   single: Container

.. summary-start

Selects the structured data container that receives fields added by mmdblookup.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: Container
:Scope: module
:Type: string (word)
:Default: module=!iplocation
:Required?: no
:Introduced: 8.28.0

Description
-----------
.. versionadded:: 8.28.0

   Specifies the container to be used to store the fields amended by
   mmdblookup.

Module usage
------------
.. _param-mmdblookup-module-container:
.. _mmdblookup.parameter.module.container-usage:

.. code-block:: rsyslog

   module(load="mmdblookup" container="!geo_ip")

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
