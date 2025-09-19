.. _param-mmdblookup-container:
.. _mmdblookup.parameter.module.container:

container
=========

.. index::
   single: mmdblookup; container
   single: container

.. summary-start

Selects the structured data container that receives fields added by mmdblookup.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: container
:Scope: module
:Type: string (word)
:Default: module=!iplocation
:Required?: no
:Introduced: 8.28.0

Description
-----------
.. versionadded:: 8.28.0

   Specifies the container to be used to store the fields amended by
   mmdblookup. This parameter specifies the name of the JSON container
   (a JSON object) where the looked-up fields will be stored within the
   rsyslog message. If not specified, it defaults to ``!iplocation``.

Module usage
------------
.. _param-mmdblookup-module-container:
.. _mmdblookup.parameter.module.container-usage:

.. code-block:: rsyslog

   module(load="mmdblookup" container="!geo_ip")

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
