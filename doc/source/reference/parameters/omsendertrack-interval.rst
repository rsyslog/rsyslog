.. _param-omsendertrack-interval:
.. _omsendertrack.parameter.input.interval:

interval
========

.. index::
   single: omsendertrack; interval
   single: interval

.. summary-start

Sets how many seconds elapse between each write of sender statistics to the state file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: interval
:Scope: input
:Type: integer
:Default: input=60
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This parameter defines the **interval in seconds** after which the module writes the current sender statistics to the configured :ref:`statefile <param-omsendertrack-statefile>`.

A smaller ``interval`` value results in more frequent updates to the state file, reducing potential data loss in case of an unexpected system crash, but it also increases disk I/O.
A larger ``interval`` reduces I/O but means less up-to-date statistics on disk.

Input usage
-----------
.. _param-omsendertrack-input-interval:
.. _omsendertrack.parameter.input.interval-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          interval="60"
          stateFile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

