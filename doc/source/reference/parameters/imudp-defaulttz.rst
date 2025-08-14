.. _param-imudp-defaulttz:
.. _imudp.parameter.module.defaulttz:

DefaultTZ
=========

.. index::
   single: imudp; DefaultTZ
   single: DefaultTZ

.. summary-start

Experimental setting that applies a fallback timezone to legacy messages lacking one.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: DefaultTZ
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Sets a default timezone for messages without timezone information, useful with
legacy syslog formats like RFC3164. The format must be ``+/-hh:mm`` such as
``-05:00`` or ``+01:30``. Timezone names (for example ``EST``) and daylight
saving adjustments are not supported.

Input usage
-----------
.. _param-imudp-input-defaulttz:
.. _imudp.parameter.input.defaulttz:
.. code-block:: rsyslog

   input(type="imudp" DefaultTZ="...")

Notes
-----
- Experimental: syntax and behavior may change or disappear without notice.
- Invalid formats may lead to malformed timestamps or even ``rsyslogd`` crashes.

See also
--------
See also :doc:`../../configuration/modules/imudp`.

