.. _param-mmcount-appname:
.. _mmcount.parameter.module.appname:

appName
=======

.. index::
   single: mmcount; appName
   single: appName

.. summary-start

Selects the application name whose messages the mmcount action tracks.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmcount`.

:Name: appName
:Scope: module
:Type: word
:Default: none
:Required?: yes
:Introduced: 7.5.0

Description
-----------
Specifies the syslog APP-NAME that must match for mmcount to process a
message. When the application name of the current message does not match
``appName``, the action immediately returns without altering counters. If
no match is found, no ``mmcount`` property is added to the message.

For matching messages, mmcount increments counters based on the
additional parameters. When :ref:`param-mmcount-key` is omitted, the
module counts per-severity totals for the selected application. If
:ref:`param-mmcount-key` is provided, counters are updated according to
that parameter's logic. The updated total is written into the message's
``mmcount`` JSON property so later actions can react to it.

Module usage
------------
.. _param-mmcount-module-appname:
.. _mmcount.parameter.module.appname-usage:

.. code-block:: rsyslog

   action(type="mmcount" appName="gluster")

See also
--------
See also :doc:`../../configuration/modules/mmcount`.
