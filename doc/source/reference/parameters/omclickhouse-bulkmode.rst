.. _param-omclickhouse-bulkmode:
.. _omclickhouse.parameter.module.bulkmode:

bulkmode
========

.. index::
   single: omclickhouse; bulkmode
   single: bulkmode

.. summary-start

Determines whether events are sent individually or batched together in bulk HTTP requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: bulkmode
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: not specified

Description
-----------
The "off" setting means logs are shipped one by one. Each in its own HTTP request. The default "on" will send multiple logs in the same request. This is recommended, because it is many times faster than when bulkmode is turned off. The maximum number of logs sent in a single bulk request depends on your maxbytes and queue settings - usually limited by the `dequeue batch size <http://www.rsyslog.com/doc/node35.html>`_. More information about queues can be found `here <http://www.rsyslog.com/doc/node32.html>`_.

Module usage
------------
.. _param-omclickhouse-module-bulkmode:
.. _omclickhouse.parameter.module.bulkmode-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" bulkmode="off")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
