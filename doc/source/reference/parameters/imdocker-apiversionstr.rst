.. _param-imdocker-apiversionstr:
.. _imdocker.parameter.module.apiversionstr:

ApiVersionStr
=============

.. index::
   single: imdocker; ApiVersionStr
   single: ApiVersionStr

.. summary-start

Docker API version string like ``v1.27``.

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
Specifies the version of Docker API to use. The value must match the format
required by the Docker API, such as ``v1.27``.

Module usage
------------
.. _param-imdocker-module-apiversionstr:
.. _imdocker.parameter.module.apiversionstr-usage:

.. code-block:: rsyslog

   module(load="imdocker" ApiVersionStr="...")

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

