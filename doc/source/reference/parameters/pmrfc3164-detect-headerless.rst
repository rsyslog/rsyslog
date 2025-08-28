.. _param-pmrfc3164-detect-headerless:
.. _pmrfc3164.parameter.module.detect-headerless:
.. _pmrfc3164.parameter.module.detect.headerless:

detect.headerless
=================

.. index::
   single: pmrfc3164; detect.headerless
   single: detect.headerless

.. summary-start

Enable detection of messages lacking standard syslog headers.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/pmrfc3164`.

:Name: detect.headerless
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2508.0

Description
-----------
When enabled, rsyslog applies extra heuristics to classify messages without a syslog header. A message is considered headerless only if:

- It has no PRI header (facility/severity).
- It does not start with an accepted timestamp.
- It does not begin with whitespace followed by ``{`` or ``[``.

Messages starting with ``{`` or ``[`` are treated as structured data (e.g., JSON) instead.

Module usage
------------

.. _param-pmrfc3164-module-detect-headerless:
.. _pmrfc3164.parameter.module.detect-headerless-usage:
.. code-block:: rsyslog

   parser(name="custom.rfc3164" type="pmrfc3164" detect.headerless="on")

Notes
-----
- Legacy docs referred to this as a ``binary`` option, which maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/pmrfc3164`.
