.. _param-pmrfc3164-detect-yearaftertimestamp:
.. _pmrfc3164.parameter.module.detect-yearaftertimestamp:
.. _pmrfc3164.parameter.module.detect.YearAfterTimestamp:

detect.YearAfterTimestamp
=========================

.. index::
   single: pmrfc3164; detect.YearAfterTimestamp
   single: detect.YearAfterTimestamp

.. summary-start

Treat a year following the timestamp as part of the timestamp instead of the hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: detect.YearAfterTimestamp
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Some devices append the year directly after the timestamp, which would otherwise be parsed as the hostname. When enabled, years between 2000 and 2099 are treated as part of the timestamp.

Module usage
------------

.. _param-pmrfc3164-module-detect-yearaftertimestamp:
.. _pmrfc3164.parameter.module.detect-yearaftertimestamp-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" detect.YearAfterTimestamp="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
