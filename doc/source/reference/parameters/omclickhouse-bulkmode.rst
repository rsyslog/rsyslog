.. _param-omclickhouse-bulkmode:
.. _omclickhouse.parameter.input.bulkmode:

bulkMode
========

.. index::
   single: omclickhouse; bulkMode
   single: bulkMode
   single: omclickhouse; bulkmode
   single: bulkmode

.. summary-start

Determines whether events are sent individually or batched together in bulk HTTP requests.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: bulkMode
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: not specified

Description
-----------
The "off" setting means logs are shipped one by one, each in its own HTTP request. The default "on" will send multiple logs in the same request. This is recommended, because it is many times faster than when ``bulkMode`` is turned off. The maximum number of logs sent in a single bulk request depends on your ``maxBytes`` and queue settings - usually limited by the `dequeue batch size <http://www.rsyslog.com/doc/node35.html>`_. More information about queues can be found `here <http://www.rsyslog.com/doc/node32.html>`_.

Input usage
-----------
.. _omclickhouse.parameter.input.bulkmode-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" bulkMode="off")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
