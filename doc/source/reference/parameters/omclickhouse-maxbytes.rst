.. _param-omclickhouse-maxbytes:
.. _omclickhouse.parameter.input.maxbytes:

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
:Scope: input
:Type: size
:Default: 104857600/100mb
:Required?: no
:Introduced: not specified

Description
-----------
When shipping logs with ``bulkMode`` **on**, ``maxBytes`` specifies the maximum size of the request body sent to ClickHouse. Logs are batched until either the buffer reaches ``maxBytes`` or the `dequeue batch size <http://www.rsyslog.com/doc/node35.html>`_ is reached.

Input usage
-----------
.. _omclickhouse.parameter.input.maxbytes-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" maxBytes="64mb")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
