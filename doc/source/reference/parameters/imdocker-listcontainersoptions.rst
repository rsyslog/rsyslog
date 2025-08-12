.. _param-imdocker-listcontainersoptions:
.. _imdocker.parameter.module.listcontainersoptions:

.. index::
   single: imdocker; ListContainersOptions
   single: ListContainersOptions

.. summary-start

Supplies query parameters for the ``List Containers`` Docker API request.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: ListContainersOptions
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=""
:Required?: no
:Introduced: 8.41.0

Description
-----------
Supplies the HTTP query component for a ``List Containers`` API call. The leading ``?`` must not be included.

.. _param-imdocker-module-listcontainersoptions:
.. _imdocker.parameter.module.listcontainersoptions-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" ListContainersOptions="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
