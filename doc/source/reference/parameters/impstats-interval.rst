.. _param-impstats-interval:
.. _impstats.parameter.module.interval:

Interval
========

.. index::
   single: impstats; Interval
   single: Interval

.. summary-start

Configures how often, in seconds, impstats emits statistics messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: Interval
:Scope: module
:Type: integer (seconds)
:Default: module=300
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the interval, in **seconds**, at which messages are generated. Please note
that the actual interval may be a bit longer. We do not try to be precise and
so the interval is actually a sleep period which is entered after generating
all messages. So the actual interval is what is configured here plus the actual
time required to generate messages. In general, the difference should not
really matter.

Module usage
------------
.. _param-impstats-module-interval:
.. _impstats.parameter.module.interval-usage:

.. code-block:: rsyslog

   module(load="impstats" interval="600")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
