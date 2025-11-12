.. _param-impstats-resetcounters:
.. _impstats.parameter.module.resetcounters:

resetcounters
=============

.. index::
   single: impstats; resetcounters
   single: resetcounters

.. summary-start

Controls whether impstats resets counters after emitting them so they show
deltas.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: resetcounters
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
When set to ``on``, counters are automatically reset after they are emitted. In
that case, they contain only deltas to the last value emitted. When set to
``off``, counters always accumulate their values. Note that in auto-reset mode
not all counters can be reset. Some counters (like queue size) are directly
obtained from internal objects and cannot be modified. Also, auto-resetting
introduces some additional slight inaccuracies due to the multi-threaded nature
of rsyslog and the fact that for performance reasons it cannot serialize access
to counter variables. As an alternative to auto-reset mode, you can use
rsyslog's statistics manipulation scripts to create delta values from the
regular statistic logs. This is the suggested method if deltas are not
necessarily needed in real-time.

Module usage
------------
.. _impstats.parameter.module.resetcounters-usage:

.. code-block:: rsyslog

   module(load="impstats" resetCounters="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
