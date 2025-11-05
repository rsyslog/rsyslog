.. _param-omazureeventhubs-statsname:
.. _omazureeventhubs.parameter.input.statsname:

statsname
=========

.. index::
   single: omazureeventhubs; statsname
   single: statsname

.. summary-start

Names the statistics counters that track this action instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: statsname
:Scope: input
:Type: word
:Default: input=omazureeventhubs
:Required?: no
:Introduced: v8.2304

Description
-----------
The name assigned to statistics specific to this action instance. The supported
set of statistics tracked for this action instance are ``submitted``,
``accepted``, ``failures`` and ``othererrors``. Note that the global module
statistics use the counter name ``failures_other``.

Input usage
-----------
.. _omazureeventhubs.parameter.input.statsname-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" statsName="custom-azure" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
