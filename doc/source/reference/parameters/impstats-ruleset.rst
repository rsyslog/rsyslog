.. _param-impstats-ruleset:
.. _impstats.parameter.module.ruleset:

ruleset
=======

.. index::
   single: impstats; ruleset
   single: ruleset

.. summary-start

Binds impstats-generated messages to the specified ruleset for further processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: ruleset
:Scope: module
:Type: string
:Default: module=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Binds the listener to a specific :doc:`ruleset <../../concepts/multi_ruleset>`.

**Note** that setting ``ruleset`` and setting :ref:`param-impstats-log-syslog`
(``logSyslog`` in camelCase examples) to ``off`` are mutually exclusive because
syslog stream processing must be enabled to use a ruleset.

Module usage
------------
.. _impstats.parameter.module.ruleset-usage:

.. code-block:: rsyslog

   module(load="impstats" ruleset="impstatsRules")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
