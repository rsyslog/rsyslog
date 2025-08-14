.. _param-imudp-defaulttz:
.. _imudp.parameter.input.defaulttz:

DefaultTZ
=========

.. index::
   single: imudp; DefaultTZ
   single: DefaultTZ

.. summary-start

Experimental default timezone applied when none present.

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
This is an **experimental** parameter; details may change at any time and it may
also be discontinued without any early warning. Permits to set a default
timezone for this listener. This is useful when working with legacy syslog
(RFC3164 et al) residing in different timezones. If set it will be used as
timezone for all messages **that do not contain timezone info**. Currently, the
format **must** be ``+/-hh:mm``, e.g. ``-05:00``, ``+01:30``. Other formats,
including TZ names (like EST) are NOT yet supported. Note that consequently no
daylight saving settings are evaluated when working with timezones. If an
invalid format is used, "interesting" things can happen, among them malformed
timestamps and ``rsyslogd`` segfaults. This will obviously be changed at the time
this feature becomes non-experimental.

Input usage
-----------
.. _param-imudp-input-defaulttz:
.. _imudp.parameter.input.defaulttz-usage:

.. code-block:: rsyslog

   input(type="imudp" DefaultTZ="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
