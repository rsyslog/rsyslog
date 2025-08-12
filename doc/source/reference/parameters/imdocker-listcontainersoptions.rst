.. _param-imdocker-listcontainersoptions:
.. _imdocker.parameter.module.listcontainersoptions:

ListContainersOptions
=====================

.. index::
   single: imdocker; ListContainersOptions
   single: ListContainersOptions

.. summary-start

HTTP query options appended to ``List Containers`` requests; omit leading ``?``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: ListContainersOptions
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies the HTTP query component of a ``List Containers`` API request. See the
Docker API for available options. It is not necessary to prepend ``?``.

Module usage
------------
.. _param-imdocker-module-listcontainersoptions:
.. _imdocker.parameter.module.listcontainersoptions-usage:

.. code-block:: rsyslog

   module(load="imdocker" ListContainersOptions="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

