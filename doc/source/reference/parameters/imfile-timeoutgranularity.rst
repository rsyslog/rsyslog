.. _param-imfile-timeoutgranularity:
.. _imfile.parameter.module.timeoutgranularity:
.. _imfile.parameter.timeoutgranularity:

timeoutGranularity
==================

.. index::
   single: imfile; timeoutGranularity
   single: timeoutGranularity

.. summary-start

Interval in seconds for checking multi-line read timeouts.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: timeoutGranularity
:Scope: module
:Type: integer
:Default: module=1
:Required?: no
:Introduced: 8.23.0

Description
-----------
Sets the interval in which multi-line-read timeouts are checked. The interval is
specified in seconds. This establishes a lower limit on the length of the
timeout. For example, if ``timeoutGranularity`` is 60 seconds and a
``readTimeout`` value of 10 seconds is used, the timeout is nevertheless only
checked every 60 seconds (if there is no other activity in imfile). This means
that the ``readTimeout`` is also only checked every 60 seconds, so a timeout can
occur only after 60 seconds.

Note that ``timeoutGranularity`` has some performance implication. The more
frequently timeout processing is triggered, the more processing time is needed.
This effect should be negligible, except if a very large number of files is
being monitored.

Module usage
------------
.. _param-imfile-module-timeoutgranularity:
.. _imfile.parameter.module.timeoutgranularity-usage:

.. code-block:: rsyslog

   module(load="imfile" timeoutGranularity="1")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
