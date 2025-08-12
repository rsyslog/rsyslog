.. _param-imdocker-getcontainerlogoptions:
.. _imdocker.parameter.module.getcontainerlogoptions:

.. index::
   single: imdocker; GetContainerLogOptions
   single: GetContainerLogOptions

.. summary-start

Supplies query parameters for the ``Get container logs`` Docker API request.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: GetContainerLogOptions
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=timestamp=0&follow=1&stdout=1&stderr=1&tail=1
:Required?: no
:Introduced: 8.41.0

Description
-----------
Supplies the HTTP query component for a ``Get container logs`` API call. The leading ``?`` must not be included.

.. _param-imdocker-module-getcontainerlogoptions:
.. _imdocker.parameter.module.getcontainerlogoptions-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" GetContainerLogOptions="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
