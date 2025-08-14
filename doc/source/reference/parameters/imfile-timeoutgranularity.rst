.. _param-imfile-timeoutgranularity:
.. _imfile.parameter.module.timeoutgranularity:

timeoutGranularity
==================

.. index::
   single: imfile; timeoutGranularity
   single: timeoutGranularity

.. summary-start

.

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
.. versionadded:: 8.23.0

This sets the interval in which multi-line-read timeouts are checked.
The interval is specified in seconds. Note that
this establishes a lower limit on the length of the timeout. For example, if
a timeoutGranularity of 60 seconds is selected and a readTimeout value of 10 seconds
is used, the timeout is nevertheless only checked every 60 seconds (if there is
no other activity in imfile). This means that the readTimeout is also only
checked every 60 seconds, which in turn means a timeout can occur only after 60
seconds.

Note that timeGranularity has some performance implication. The more frequently
timeout processing is triggered, the more processing time is needed. This
effect should be negligible, except if a very large number of files is being
monitored.

Module usage
------------
.. _param-imfile-module-timeoutgranularity:
.. _imfile.parameter.module.timeoutgranularity-usage:
.. code-block:: rsyslog

   module(load="imfile" timeoutGranularity="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
