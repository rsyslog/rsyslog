.. _param-imdocker-defaultfacility:
.. _imdocker.parameter.module.defaultfacility:

DefaultFacility
===============

.. index::
   single: imdocker; DefaultFacility
   single: DefaultFacility

.. summary-start

Syslog facility assigned to received messages; default ``user``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DefaultFacility
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=user
:Required?: no
:Introduced: 8.41.0

Description
-----------
The syslog facility to be assigned to log messages. Textual names such as
``user`` are suggested, though numeric values are also accepted.

Module usage
------------
.. _param-imdocker-module-defaultfacility:
.. _imdocker.parameter.module.defaultfacility-usage:

.. code-block:: rsyslog

   module(load="imdocker" DefaultFacility="...")

Notes
-----
- Numeric facility values are accepted but textual names are recommended.
- See https://en.wikipedia.org/wiki/Syslog.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

