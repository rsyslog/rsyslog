.. _param-omprog-reportfailures:
.. _omprog.parameter.action.reportfailures:

reportFailures
==============

.. index::
   single: omprog; reportFailures
   single: reportFailures

.. summary-start

Logs a warning when the program signals an error while confirming messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: reportFailures
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.38.0

Description
-----------
Specifies whether rsyslog must internally log a warning message whenever the
program returns an error when confirming a message. The logged message will
include the error line returned by the program. This parameter is ignored when
:ref:`param-omprog-confirmmessages` is set to "off".

Enabling this flag can be useful to log the problems detected by the program.
However, the information that can be logged is limited to a short error line,
and the logs will be tagged as originated by the 'syslog' facility (like the
rest of rsyslog logs). To avoid these shortcomings, consider the use of the
:ref:`param-omprog-output` parameter to capture the stderr of the program.

Action usage
------------
.. _param-omprog-action-reportfailures:
.. _omprog.parameter.action.reportfailures-usage:

.. code-block:: rsyslog

   action(type="omprog"
          confirmMessages="on"
          reportFailures="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
