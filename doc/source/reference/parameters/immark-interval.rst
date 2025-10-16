.. _param-immark-interval:
.. _immark.parameter.module.interval:

interval
========

.. index::
   single: immark; interval
   single: interval

.. summary-start

Specifies how often immark injects a mark message, in seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/immark`.

:Name: interval
:Scope: module
:Type: integer (seconds)
:Default: module=1200 (seconds)
:Required?: no
:Introduced: 3.0.0

Module usage
------------
.. _immark.parameter.module.interval-usage:

.. code-block:: rsyslog

   module(load="immark" interval="1200")

Legacy names (for reference)
----------------------------
Historic names/directives for compatibility. Do not use in new configs.

.. _immark.parameter.legacy.markmessageperiod:

- ``$MarkMessagePeriod`` — maps to interval (status: legacy)

.. index::
   single: immark; $MarkMessagePeriod
   single: $MarkMessagePeriod

See also
--------

.. seealso::

   * ``action.writeAllMarkMessages`` in :doc:`../../configuration/actions`
   * :doc:`../../configuration/modules/immark`
