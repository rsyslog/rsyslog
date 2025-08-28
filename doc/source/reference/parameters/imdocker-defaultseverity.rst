.. _param-imdocker-defaultseverity:
.. _imdocker.parameter.module.defaultseverity:

DefaultSeverity
===============

.. index::
   single: imdocker; DefaultSeverity
   single: DefaultSeverity

.. summary-start

Syslog severity assigned to received messages; default ``info``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: DefaultSeverity
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=info
:Required?: no
:Introduced: 8.41.0

Description
-----------
The syslog severity to be assigned to log messages. Textual names such as
``info`` are suggested, though numeric values are also accepted.

Module usage
------------
.. _param-imdocker-module-defaultseverity:
.. _imdocker.parameter.module.defaultseverity-usage:

.. code-block:: rsyslog

   module(load="imdocker" DefaultSeverity="...")

Notes
-----
- Numeric severity values are accepted but textual names are recommended.
- See https://en.wikipedia.org/wiki/Syslog.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

