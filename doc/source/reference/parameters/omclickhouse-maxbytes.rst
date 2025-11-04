.. _param-omclickhouse-maxbytes:
.. _omclickhouse.parameter.module.maxbytes:

maxbytes
========

.. index::
   single: omclickhouse; maxbytes
   single: maxbytes

.. summary-start

Limits the maximum size of a bulk HTTP request when bulkmode is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: maxbytes
:Scope: module
:Type: size
:Default: module=104857600/100mb
:Required?: no
:Introduced: not specified

Description
-----------
When shipping logs with bulkmode **on**, maxbytes specifies the maximum size of the request body sent to ClickHouse. Logs are batched until either the buffer reaches maxbytes or the `dequeue batch size <http://www.rsyslog.com/doc/node35.html>`_ is reached.

Module usage
------------
.. _param-omclickhouse-module-maxbytes:
.. _omclickhouse.parameter.module.maxbytes-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" maxbytes="64mb")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
