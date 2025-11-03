.. _param-omazureeventhubs-statsname:
.. _omazureeventhubs.parameter.module.statsname:

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
:Scope: module
:Type: word
:Default: module=omazureeventhubs
:Required?: no
:Introduced: v8.2304

Description
-----------
The name assigned to statistics specific to this action instance. The supported
set of statistics tracked for this action instance are ``submitted``,
``accepted``, ``failures`` and ``failures_other``. See the
:ref:`statistics-counter_omazureeventhubs_label` section for more details.

Module usage
------------
.. _param-omazureeventhubs-module-statsname:
.. _omazureeventhubs.parameter.module.statsname-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" statsName="custom-azure" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
