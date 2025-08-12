.. _param-imdocker-apiversionstr:
.. _imdocker.parameter.module.apiversionstr:

.. index::
   single: imdocker; ApiVersionStr
   single: ApiVersionStr

.. summary-start

Specifies the Docker API version string to use.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: ApiVersionStr
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=v1.27
:Required?: no
:Introduced: 8.41.0

Description
-----------
Specifies the Docker API version string to use. It must match the format required by the Docker API.

.. _param-imdocker-module-apiversionstr:
.. _imdocker.parameter.module.apiversionstr-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" ApiVersionStr="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
