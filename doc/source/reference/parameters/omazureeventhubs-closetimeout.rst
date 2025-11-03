.. _param-omazureeventhubs-closetimeout:
.. _omazureeventhubs.parameter.input.closetimeout:

closeTimeout
============

.. index::
   single: omazureeventhubs; closeTimeout
   single: closeTimeout

.. summary-start

Controls how long the action waits for Event Hubs responses before closing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazureeventhubs`.

:Name: closeTimeout
:Scope: input
:Type: integer
:Default: input=2000
:Required?: no
:Introduced: v8.2304

Description
-----------
The close timeout configuration property is used in the rsyslog output module to
specify the amount of time the output module should wait for a response from
Microsoft Azure Event Hubs before timing out and closing the connection.

This property is used to control the amount of time the output module will wait
for a response from the target Event Hubs instance before giving up and assuming
that the connection has failed. The close timeout property is specified in
milliseconds.

Input usage
-----------
.. _omazureeventhubs.parameter.input.closetimeout-usage:

.. code-block:: rsyslog

   action(type="omazureeventhubs" closeTimeout="2000" ...)

See also
--------
See also :doc:`../../configuration/modules/omazureeventhubs`.
