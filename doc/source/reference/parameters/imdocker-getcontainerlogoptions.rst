.. _param-imdocker-getcontainerlogoptions:
.. _imdocker.parameter.module.getcontainerlogoptions:

GetContainerLogOptions
======================

.. index::
   single: imdocker; GetContainerLogOptions
   single: GetContainerLogOptions

.. summary-start

HTTP query options for ``Get container logs`` requests; default ``timestamps=0&follow=1&stdout=1&stderr=1&tail=1``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: GetContainerLogOptions
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=timestamps=0&follow=1&stdout=1&stderr=1&tail=1
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies the HTTP query component of a ``Get container logs`` API request. See
the Docker API for available options. It is not necessary to prepend ``?``.

Module usage
------------
.. _param-imdocker-module-getcontainerlogoptions:
.. _imdocker.parameter.module.getcontainerlogoptions-usage:

.. code-block:: rsyslog

   module(load="imdocker" GetContainerLogOptions="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

