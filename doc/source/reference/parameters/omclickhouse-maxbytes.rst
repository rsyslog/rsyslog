.. _param-omclickhouse-maxbytes:
.. _omclickhouse.parameter.module.maxbytes:

maxBytes
========

.. index::
   single: omclickhouse; maxBytes
   single: maxBytes
   single: omclickhouse; maxbytes
   single: maxbytes

.. summary-start

Limits the maximum size of a bulk HTTP request when ``bulkMode`` is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: maxBytes
:Scope: module
:Type: size
:Default: module=104857600/100mb
:Required?: no
:Introduced: not specified

Description
-----------
When shipping logs with ``bulkMode`` **on**, ``maxBytes`` specifies the maximum size of the request body sent to ClickHouse. Logs are batched until either the buffer reaches ``maxBytes`` or the `dequeue batch size <http://www.rsyslog.com/doc/node35.html>`_ is reached.

Module usage
------------
.. _param-omclickhouse-module-maxbytes:
.. _omclickhouse.parameter.module.maxbytes-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" maxBytes="64mb")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
