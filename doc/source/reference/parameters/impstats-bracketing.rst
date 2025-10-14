.. _param-impstats-bracketing:
.. _impstats.parameter.module.bracketing:

Bracketing
==========

.. index::
   single: impstats; Bracketing
   single: Bracketing

.. summary-start

Adds BEGIN and END marker messages so post-processing can group statistics blocks.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: Bracketing
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.4.1

Description
-----------
.. versionadded:: 8.4.1

This is a utility setting for folks who post-process impstats logs and would
like to know the begin and end of a block of statistics. When ``bracketing`` is
set to ``on``, impstats issues a ``BEGIN`` message before the first counter is
issued, then all counter values are issued, and then an ``END`` message follows.
As such, if and only if messages are kept in sequence, a block of stats counts
can easily be identified by those BEGIN and END messages.

**Note well:** in general, sequence of syslog messages is **not** strict and is
not ordered in sequence of message generation. There are various occasion that
can cause message reordering, some examples are:

* using multiple threads
* using UDP forwarding
* using relay systems, especially with buffering enabled
* using disk-assisted queues

This is not a problem with rsyslog, but rather the way a concurrent world works.
For strict order, a specific order predicate (e.g. a sufficiently fine-grained
timestamp) must be used.

As such, BEGIN and END records may actually indicate the begin and end of a
block of statistics - or they may *not*. Any order is possible in theory. So the
bracketing option does not in all cases work as expected. This is the reason why
it is turned off by default.

*However*, bracketing may still be useful for many use cases. First and
foremost, while there are many scenarios in which messages become reordered, in
practice it happens relatively seldom. So most of the time the statistics
records will come in as expected and actually will be bracketed by the BEGIN and
END messages. Consequently, if an application can handle occasional out-of-order
delivery (e.g. by graceful degradation), bracketing may actually be a great
solution. It is, however, very important to know and handle out of order
delivery. For most real-world deployments, a good way to handle it is to ignore
unexpected records and use the previous values for ones missing in the current
block. To guard against two or more blocks being mixed, it may also be a good
idea to never reset a value to a lower bound, except when that lower bound is
seen consistently (which happens due to a restart). Note that such lower bound
logic requires ``resetCounters`` to be set to ``off``.

Module usage
------------
.. _impstats.parameter.module.bracketing-usage:

.. code-block:: rsyslog

   module(load="impstats" bracketing="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
